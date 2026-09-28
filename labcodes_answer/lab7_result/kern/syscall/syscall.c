#include <defs.h>
#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>
#include <clock.h>
#include <error.h>
#include <vmm.h>
#include <agent_tool.h>
#include <string.h>
#include <console.h>

#define AGENT_CTX_VA    0x20000000
#define AGENT_CTX_SIZE  PGSIZE
struct agent_info {
    int agent_type;
    int heartbeat_interval;
    size_t resource_quota;
    int loop_state;
    uintptr_t agent_ctx_va;
    size_t ctx_total_len;
    size_t ctx_cur_pos;
} __attribute__((packed));




static int
sys_exit(uint32_t arg[]) {
    int error_code = (int)arg[0];
    return do_exit(error_code);
}

static int
sys_fork(uint32_t arg[]) {
    struct trapframe *tf = current->tf;
    uintptr_t stack = tf->tf_esp;
    return do_fork(0, stack, tf);
}

static int
sys_wait(uint32_t arg[]) {
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    return do_wait(pid, store);
}

static int
sys_exec(uint32_t arg[]) {
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];
    return do_execve(name, len, binary, size);
}

static int
sys_yield(uint32_t arg[]) {
    return do_yield();
}

static int
sys_kill(uint32_t arg[]) {
    int pid = (int)arg[0];
    return do_kill(pid);
}

static int
sys_getpid(uint32_t arg[]) {
    return current->pid;
}

static int
sys_putc(uint32_t arg[]) {
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}

static int
sys_pgdir(uint32_t arg[]) {
    print_pgdir();
    return 0;
}

static int
sys_gettime(uint32_t arg[]) {
    return (int)ticks;
}
static int
sys_lab6_set_priority(uint32_t arg[])
{
    uint32_t priority = (uint32_t)arg[0];
    lab6_set_priority(priority);
    return 0;
}

static int
sys_sleep(uint32_t arg[]) {
    unsigned int time = (unsigned int)arg[0];
    return do_sleep(time);
}

// Agent‑OS 任务一
static int
sys_agent_create(uint32_t arg[])
{
    int heartbeat = (int)arg[0];
    size_t quota = (size_t)arg[1];
    int pid = do_fork(0, current->tf->tf_esp, current->tf);
    if (pid < 0) {
        return pid;
    }
    struct proc_struct *child = find_proc(pid);
    if (child == NULL) {
        return -E_INVAL;
    }
    // 修改为Agent进程
    child->agent_type = AGENT_TYPE_AGENT;
    child->heartbeat_interval = heartbeat;
    child->resource_quota = quota;
    child->context_path_meta.quota = quota;
    child->context_path_meta.evict_policy = 0;
    child->context_path_meta.node_count = 0;
    child->context_path_meta.total_len = 0;
    child->context_path_meta.cur_pos = 0;
    child->loop_state = AGENT_LOOP_IDLE;
    int ret = mm_map(child->mm, AGENT_CTX_VA, AGENT_CTX_SIZE, VM_READ | VM_WRITE, NULL);
    if (ret != 0) {
        do_kill(pid);
        return -1;
    }
    child->agent_ctx_va = AGENT_CTX_VA;
    cprintf("sys_agent_create: create pid %d, set agent_type=%d\n", pid, child->agent_type);
        // 任务二：初始化Agent消息缓冲区
    child->agent_msg_len = 0;
    memset(child->agent_msg_buf, 0, AGENT_MSG_BUF_LEN);

    return pid;
}



static int
sys_agent_info(uint32_t arg[])
{
    int pid = (int)arg[0];
    uintptr_t user_va = arg[1];
    cprintf("sys_agent_info: pid=%d, user buffer va=0x%08x, kernel agent_info size=%d\n", pid, user_va, (int)sizeof(struct agent_info));

    struct proc_struct *proc = find_proc(pid);
    if (proc == NULL) {
        cprintf("sys_agent_info: cannot find pid %d\n", pid);
        return -E_INVAL;
    }
    cprintf("sys_agent_info: found pid %d, state=%d, agent_type=%d, AGENT_TYPE_AGENT=%d\n",
            pid, proc->state, proc->agent_type, AGENT_TYPE_AGENT);
    if (proc->agent_type != AGENT_TYPE_AGENT) {
        cprintf("sys_agent_info: ERROR, NOT agent!\n");
        return -E_INVAL;
    }

    struct agent_info k_info;
    k_info.agent_type = proc->agent_type;
    k_info.heartbeat_interval = proc->heartbeat_interval;
    k_info.resource_quota = proc->resource_quota;
    k_info.loop_state = proc->loop_state;
    k_info.agent_ctx_va = proc->agent_ctx_va;
    k_info.ctx_total_len = proc->context_path_meta.total_len;
    k_info.ctx_cur_pos = proc->context_path_meta.cur_pos;

   bool ret = copy_to_user(current->mm, (void *)user_va, &k_info, sizeof(struct agent_info));
    if (!ret) { // ret ==0 代表校验失败
        cprintf("sys_agent_info: copy_to_user failed\n");
        return -E_INVAL;
    }
cprintf("sys_agent_info: copy success\n");

    return 0;
}

//任务三新增FIFO淘汰策略函数
static int fifo_evict(struct proc_struct *proc)
{
    if (proc->context_path_meta.node_count == 0) {
        return -E_INVAL;
    }
    size_t node_sz = sizeof(struct context_node);
    // 淘汰最老的节点，即从agent_ctx_va开始的第一个节点
    cprintf("[FIFO_EVICT] trigger evict, drop oldest node. node_count=%d, total_len=%u\n",proc->context_path_meta.node_count, proc->context_path_meta.total_len);
    uintptr_t base_va = proc->agent_ctx_va;
    size_t move_bytes = proc->context_path_meta.total_len - node_sz;

    char tmp_buf[AGENT_CTX_SIZE];

    // copy_from_user：5个参数，末尾writable传0：只是读用户内存，不需要写权限
    bool ok = copy_from_user(proc->mm, tmp_buf, (void *)(base_va + node_sz), move_bytes, 0);
    if (!ok) {
        return -E_FAULT;
    }
    // copy_to_user：4个参数
    ok = copy_to_user(proc->mm, (void *)base_va, tmp_buf, move_bytes);
    if (!ok) {
        return -E_FAULT;
    }

    proc->context_path_meta.total_len -= node_sz;
    proc->context_path_meta.node_count --;
    proc->context_path_meta.cur_pos -= node_sz;
    //淘汰结束后
    cprintf("[FIFO_EVICT] evict done. new node_count=%d, total_len=%u\n",proc->context_path_meta.node_count, proc->context_path_meta.total_len);
    return 0;
}

//任务三的4个系统调用
static int sys_context_push(uint32_t arg[])
{
    cprintf("[sys_context_push] enter, pid=%d\n", current->pid);
    if (current->agent_type != AGENT_TYPE_AGENT) {
        cprintf("[sys_context_push] ERR: not agent type\n");
        return -E_PERM;
    }
    // arg[0]：用户态 struct context_node* 虚拟地址
    uintptr_t node_va = arg[0];
    struct context_node node;
    // 从用户态读取待写入节点，只读，writable=0
    bool ok = copy_from_user(current->mm, &node, (void *)node_va, sizeof(struct context_node), 0);
    if (!ok) {
        cprintf("[sys_context_push] ERR copy_from_user\n");
        return -E_FAULT;
    }
    size_t node_sz = sizeof(struct context_node);
    // 空间不够，循环淘汰
    while(current->context_path_meta.total_len + node_sz > current->resource_quota)
    {
        cprintf("[sys_context_push] quota full, trigger fifo evict\n");
        int ret = fifo_evict(current);
        if(ret != 0){
            cprintf("[sys_context_push] fifo evict failed ret=%d\n", ret);
            return ret;
        }
    }
    // 把节点拷贝到用户态agent_ctx_va + cur_pos
    uintptr_t dest_va = current->agent_ctx_va + current->context_path_meta.cur_pos;
    ok = copy_to_user(current->mm, (void *)dest_va, &node, node_sz);
    if (!ok) {
        cprintf("[sys_context_push] ERR copy_to_user\n");
        return -E_FAULT;
    }
    // 更新PCB元信息
    current->context_path_meta.total_len += node_sz;
    current->context_path_meta.node_count ++;
    current->context_path_meta.cur_pos += node_sz;
    cprintf("[sys_context_push] success, node_count=%d, total_len=%u, cur_pos=%u\n",
        current->context_path_meta.node_count,
        current->context_path_meta.total_len,
        current->context_path_meta.cur_pos);
    return 0;
}
static int sys_context_query(uint32_t arg[])
{
    cprintf("[sys_context_query] enter, pid=%d\n", current->pid);
    if (current->agent_type != AGENT_TYPE_AGENT) {
        cprintf("[sys_context_query] ERR: not agent type\n");
        return -E_PERM;
    }
    // arg[0]：索引idx； arg[1]：用户输出缓冲区va
    int idx = (int)arg[0];
    uintptr_t buf_va = arg[1];
    cprintf("[sys_context_query] query idx=%d\n", idx);
    size_t node_sz = sizeof(struct context_node);
    // 索引合法性检查
    if(idx < 0 || idx >= current->context_path_meta.node_count){
        cprintf("[sys_context_query] ERR invalid idx=%d\n", idx);
        return -E_INVAL;
    }
    uintptr_t src_va = current->agent_ctx_va + idx * node_sz;
    struct context_node node;
    bool ok = copy_from_user(current->mm, &node, (void *)src_va, node_sz, 0);
    if (!ok) {
        cprintf("[sys_context_query] ERR copy_from_user\n");
        return -E_FAULT;
    }
    ok = copy_to_user(current->mm, (void *)buf_va, &node, node_sz);
    if (!ok) {
        cprintf("[sys_context_query] ERR copy_to_user\n");
        return -E_FAULT;
    }
    cprintf("[sys_context_query] success\n");
    return 0;
}
static int sys_context_rollback(uint32_t arg[])
{
    cprintf("[sys_context_rollback] enter, pid=%d\n", current->pid);
    if (current->agent_type != AGENT_TYPE_AGENT) {
        cprintf("[sys_context_rollback] ERR: not agent type\n");
        return -E_PERM;
    }
    // arg[0]：目标回退索引
    int target_idx = (int)arg[0];
    size_t node_sz = sizeof(struct context_node);
    cprintf("[sys_context_rollback] rollback to idx=%d\n", target_idx);
    if(target_idx < 0 || target_idx >= current->context_path_meta.node_count){
        cprintf("[sys_context_rollback] ERR invalid idx=%d\n", target_idx);
        return -E_INVAL;
    }
    current->context_path_meta.node_count = target_idx + 1;
    current->context_path_meta.total_len = (target_idx + 1) * node_sz;
    current->context_path_meta.cur_pos = (target_idx + 1) * node_sz;
    cprintf("[sys_context_rollback] success, node_count=%d, total_len=%u\n",
        current->context_path_meta.node_count,
        current->context_path_meta.total_len);
    return 0;
}
static int sys_context_clear(uint32_t arg[])
{
    cprintf("[sys_context_clear] enter, pid=%d\n", current->pid);
    if (current->agent_type != AGENT_TYPE_AGENT) {
        cprintf("[sys_context_clear] ERR: not agent type\n");
        return -E_PERM;
    }
    current->context_path_meta.node_count = 0;
    current->context_path_meta.total_len = 0;
    current->context_path_meta.cur_pos = 0;
    cprintf("[sys_context_clear] success, all context cleared\n");
    return 0;
}

// Agent‑OS 任务二
//===================== AgentOS 任务二：工具分发表 =====================
typedef int (*tool_handler_t)(struct tool_request *req, void *out_buf, size_t max_out, size_t *out_len);

struct tool_entry {
    const char *tool_name;
    tool_handler_t handler;
};
static const struct tool_entry tool_table[] = {
    {"query_process",    tool_query_process},
    {"get_system_status",tool_get_system_status},
    {"send_message",     tool_send_message},
    {NULL, NULL} // 结束标记
};


// 根据工具名查找handler
static tool_handler_t find_tool(const char *tool_name)
{   
    int i;
    for (i = 0; tool_table[i].tool_name != NULL; i++)
    {
        if (strcmp(tool_table[i].tool_name, tool_name) == 0)
        {
            return tool_table[i].handler;
        }
    }
    return NULL;
}

static int
sys_tool_call(uint32_t arg[])
{
    // arg[0]：用户态传入 struct tool_request 的虚拟地址 req_va
    uintptr_t req_va = arg[0];

    // ✅权限校验：仅Agent进程允许调用sys_tool_call
    if (current->agent_type != AGENT_TYPE_AGENT)
    {
        return -E_PERM;
    }

    // 1. 在内核栈分配请求结构体缓冲区，从用户态拷贝 tool_request
    struct tool_request req;
        bool ret = copy_from_user(current->mm, &req, (void *)req_va, sizeof(struct tool_request), 0);
    if (!ret)
    {
        return -E_INVAL;
    }


    // 2. 查找对应的工具handler
    tool_handler_t handler = find_tool(req.tool_name);
    if (handler == NULL)
    {
        return -E_NOENT;
    }

    // 3. 输出缓冲区：固定写入当前Agent的AGENT_CTX_VA，最大长度AGENT_CTX_SIZE
    void *out_buf = (void *)AGENT_CTX_VA;
    size_t out_len = 0;
    ret = handler(&req, out_buf, AGENT_CTX_SIZE, &out_len);

    // handler执行完成，ret是handler返回值；out_len是实际输出字节长度
    // 返回给用户态：本次输出的有效字节数
    if (ret == 0)
    {
        return out_len;
    }
    return ret;
}

static int
sys_tool_list(uint32_t arg[])
{
    uintptr_t out_len_va = arg[0]; // 用户态：uint32_t *out_result_len
    uint32_t count = 0;
    // 统计工具数量
    int i;
    for(i = 0; tool_table[i].tool_name != NULL; i++){
        count++;
    }
    // 将count拷贝回用户态指针
    int ret = copy_to_user(current->mm, (void *)out_len_va, &count, sizeof(uint32_t));
    if(!ret){
        return -E_INVAL;
    }
    return 0;
}


static int (*syscalls[])(uint32_t arg[]) = {
    [SYS_exit]              sys_exit,
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
    [SYS_pgdir]             sys_pgdir,
    [SYS_gettime]           sys_gettime,
    [SYS_lab6_set_priority] sys_lab6_set_priority,
    [SYS_sleep]             sys_sleep,
    /* ========= Agent‑OS 任务一新增 ========= */
    [SYS_agent_create]        sys_agent_create,
    [SYS_agent_info]           sys_agent_info,
    /* ========= Agent‑OS 任务二 新增 ========= */
    [SYS_tool_call]         sys_tool_call,
    [SYS_tool_list]         sys_tool_list,
    /* ========= Agent‑OS 任务三 新增 ========= */
    [SYS_context_push]      sys_context_push,
    [SYS_context_query]     sys_context_query,
    [SYS_context_rollback]  sys_context_rollback,
    [SYS_context_clear]     sys_context_clear,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void
syscall(void) {
    struct trapframe *tf = current->tf;
    uint32_t arg[5];
    int num = tf->tf_regs.reg_eax;
    if (num >= 0 && num < NUM_SYSCALLS) {
        if (syscalls[num] != NULL) {
             arg[0] = tf->tf_regs.reg_edx;
            arg[1] = tf->tf_regs.reg_ecx;
            arg[2] = tf->tf_regs.reg_ebx;
            arg[3] = tf->tf_regs.reg_edi;
            arg[4] = tf->tf_regs.reg_esi;
            tf->tf_regs.reg_eax = syscalls[num](arg);
            return ;
        }
    }
    print_trapframe(tf);
    panic("undefined syscall %d, pid = %d, name = %s.\n",
            num, current->pid, current->name);
}

