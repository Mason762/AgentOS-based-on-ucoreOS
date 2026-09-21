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

static uint32_t
sys_gettime(uint32_t arg[]) {
    return (int)ticks;
}
static uint32_t
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
    child->loop_state = AGENT_LOOP_IDLE;
    int ret = mm_map(child->mm, AGENT_CTX_VA, AGENT_CTX_SIZE, VM_READ | VM_WRITE, NULL);
    if (ret != 0) {
        do_kill(pid);
        return -1;
    }
    child->agent_ctx_va = AGENT_CTX_VA;
    cprintf("sys_agent_create: create pid %d, set agent_type=%d\n", pid, child->agent_type);
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

    int ret = copy_to_user(current->mm, (void *)user_va, &k_info, sizeof(struct agent_info));
cprintf("sys_agent_info: copy_to_user return %d\n", ret);
if (ret == 0) { // ret ==0 代表校验失败
    cprintf("sys_agent_info: copy_to_user failed\n");
    return -E_INVAL;
}
cprintf("sys_agent_info: copy success\n");

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
    /* ========= Agent‑OS 新增 ========= */
    [SYS_agent_create]        sys_agent_create,
    [SYS_agent_info]           sys_agent_info,
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

