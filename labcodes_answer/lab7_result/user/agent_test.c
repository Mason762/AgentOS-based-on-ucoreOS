#include <defs.h>
#include <ulib.h>
#include <stdio.h>
#include <syscall.h>
#include <agent_tool.h>
#include <string.h>
// 用户态结构体，必须和内核 struct agent_info 完全一致
struct agent_info {
    int agent_type;
    int heartbeat_interval;
    size_t resource_quota;
    int loop_state;
    uintptr_t agent_ctx_va;
    size_t ctx_total_len;
    size_t ctx_cur_pos;
} __attribute__((packed));

// ================= 用户态系统调用封装：任务二新增 =================
static inline int
sys_tool_call(struct tool_request *req)
{
    return syscall(SYS_tool_call, (uint32_t)req, 0,0,0,0);
}

static inline int
sys_tool_list(uint32_t *out_count)
{
    return syscall(SYS_tool_list, (uint32_t)out_count,0,0,0,0);
}

// 辅助：填充tool_param（int类型）
static void set_param_int(struct tool_param *p, const char *key, int32_t val)
{
    memset(p,0,sizeof(*p));
    strncpy(p->key, key, PARAM_KEY_MAX-1);
    p->key[PARAM_KEY_MAX-1] = '\0';
    p->val_type = PARAM_TYPE_INT;
    p->val.ival = val;
}
// 辅助：填充tool_param（string类型）
static void set_param_str(struct tool_param *p, const char *key, const char *s)
{
    memset(p,0,sizeof(*p));
    strncpy(p->key, key, PARAM_KEY_MAX-1);
    p->key[PARAM_KEY_MAX-1] = '\0';
    p->val_type = PARAM_TYPE_STRING;
    strncpy(p->val.sval, s, PARAM_STR_VAL_MAX-1);
    p->val.sval[PARAM_STR_VAL_MAX-1] = '\0';
}

#define AGENT_CTX_VA 0x20000000

int main(void)
{
    cprintf("USER MAIN START\n");
    char padding[4096];
    struct agent_info info;
    padding[0] = 0;
    info.agent_type = 0;
    cprintf("===== Agent Test Program Start =====\n");
    // ========== 测试原有 fork + waitpid ==========
    cprintf("\n[Test 1] Test original fork & waitpid\n");
    int pid_fork = fork();
    if (pid_fork < 0) {
        cprintf("fork failed!\n");
    } else if (pid_fork == 0) {
        cprintf("fork child: pid = %d, I am normal process\n", getpid());
        exit(0);
    } else {
        int ret;
        int wait_pid = waitpid(pid_fork, &ret);
        cprintf("parent: normal child pid %d exit, ret=%d\n", wait_pid, ret);
    }
    // ========== 测试 agent_create + agent_info（任务一核心） ==========
    cprintf("\n[Test 2] Test new agent_create & agent_info\n");
    int agent_pid = agent_create(10, 4096);
    if (agent_pid < 0) {
        cprintf("agent_create failed!\n");
    } else if (agent_pid == 0) {
        // Agent子进程：这里作为Agent进程，执行任务二工具调用测试
        cprintf("Agent child started, pid=%d, run task2 tool test\n", getpid());
        // ========= Test3: Task2 tool call test =========
        cprintf("\n[Test3.1] tool: get_system_status\n");
        {
            struct tool_request req;
            memset(&req,0,sizeof(req));
            strncpy(req.tool_name, "get_system_status", TOOL_NAME_MAX-1);
            req.param_cnt = 0;
            int out_len = sys_tool_call(&req);
            if(out_len < 0){
                cprintf("get_system_status failed, ret=%d\n", out_len);
            }else{
                struct tool_response_hdr *hdr = (struct tool_response_hdr *)AGENT_CTX_VA;
                cprintf("  status_code=%d, item_count=%d\n", hdr->status_code, hdr->item_count);
                if(hdr->status_code == 0 && hdr->item_count >=1){
                    struct sys_status_item *st = (struct sys_status_item *)(AGENT_CTX_VA + sizeof(struct tool_response_hdr));
                    cprintf("  total_mem=%u, free_mem=%u, proc_count=%d\n",
                            st->total_mem, st->free_mem, st->proc_count);
                }
            }
        }

        cprintf("\n[Test3.2] tool: query_process (filter agent type=1)\n");
        {
            struct tool_request req;
            memset(&req,0,sizeof(req));
            strncpy(req.tool_name, "query_process", TOOL_NAME_MAX-1);
            req.param_cnt = 1;
            set_param_int(&req.params[0], "type", 1);
            int out_len = sys_tool_call(&req);
            if(out_len <0){
                cprintf("query_process failed, ret=%d\n", out_len);
            }else{
                struct tool_response_hdr *hdr = (struct tool_response_hdr *)AGENT_CTX_VA;
                cprintf("  status_code=%d, item_count=%d\n", hdr->status_code, hdr->item_count);
                if(hdr->status_code ==0){
                    struct proc_result_item *item = (struct proc_result_item *)(AGENT_CTX_VA + sizeof(struct tool_response_hdr));
                    uint32_t i;
                    for(i=0;i<hdr->item_count;i++){
                        cprintf("    item[%d]: pid=%d, agent_type=%d, state=%d, name=%s\n",
                                i, item[i].pid, item[i].agent_type, item[i].state, item[i].name);
                    }
                }
            }
        }

        cprintf("\n[Test3.3] tool: send_message to self pid %d\n", getpid());
        {
            struct tool_request req;
            memset(&req,0,sizeof(req));
            strncpy(req.tool_name, "send_message", TOOL_NAME_MAX-1);
            req.param_cnt = 2;
            set_param_int(&req.params[0], "pid", getpid());
            set_param_str(&req.params[1], "msg", "Hello AgentOS!");
            int out_len = sys_tool_call(&req);
            if(out_len <0){
                cprintf("send_message failed, ret=%d\n", out_len);
            }else{
                struct tool_response_hdr *hdr = (struct tool_response_hdr *)AGENT_CTX_VA;
                cprintf("  status_code=%d, item_count=%d\n", hdr->status_code, hdr->item_count);
                if(hdr->status_code ==0){
                    cprintf("  send message success!\n");
                }
            }
        }
        cprintf("\n[Test3] All tool test finished\n");

        // ====================== 任务三测试【新增，任务一/二不动】======================
        cprintf("\n===== Task3 Context Syscall Test Start =====\n");
        struct context_node node_write, node_read;
        struct agent_info ag_info;

        // 循环写入多个节点，塞满配额触发FIFO淘汰
        cprintf("[Task3] Push 5 context nodes to trigger FIFO evict (quota=4096)\n");
        int i;
        for(i=0;i<5;i++){
            memset(&node_write,0,sizeof(node_write));
            // 写入区分标记，方便验证FIFO淘汰：存放序号i
            node_write.tag = i;
            int ret = sys_context_push(&node_write);
            cprintf("[Task3] push node %d, ret=%d\n", i, ret);

            // push后读取当前Agent状态，打印当前节点数量
            agent_info(getpid(), &ag_info);
            int node_cnt = ag_info.ctx_total_len / sizeof(struct context_node);
            cprintf("[Task3]  --> after push %d, node_count=%d, total_len=%u\n",
                    i, node_cnt, ag_info.ctx_total_len);
        }

        // 查询索引0，验证：最老的节点被FIFO淘汰，现在索引0是原来第1号节点
        cprintf("\n[Task3] Query index 0 (after eviction):\n");
        int q_ret = sys_context_query(0, &node_read);
        if(q_ret == 0){
            cprintf("[Task3] query ok, node.tag = %d\n", node_read.tag);
        }else{
            cprintf("[Task3] query failed ret=%d\n", q_ret);
        }

        // 回退到索引1
        cprintf("\n[Task3] Rollback to index 1\n");
        int rb_ret = sys_context_rollback(1);
        cprintf("[Task3] rollback ret=%d\n", rb_ret);
        agent_info(getpid(), &ag_info);
        int after_rollback_cnt = ag_info.ctx_total_len / sizeof(struct context_node);
        cprintf("[Task3] after rollback, node_count=%d\n", after_rollback_cnt);

        // 清空全部上下文
        cprintf("\n[Task3] Clear all context\n");
        int clr_ret = sys_context_clear();
        cprintf("[Task3] clear ret=%d\n", clr_ret);
        agent_info(getpid(), &ag_info);
        int after_clear_cnt = ag_info.ctx_total_len / sizeof(struct context_node);
        cprintf("[Task3] after clear, node_count=%d\n", after_clear_cnt);

        cprintf("\n===== Task3 Context Syscall Test Finished =====\n");
        // ===========================================================================

        exit(0);
    } else {
        cprintf("agent_create success, agent pid = %d\n", agent_pid);
        struct agent_info info;
        int info_ret = agent_info(agent_pid, &info);
        if(info_ret == 0) {
            cprintf("agent_info result:\n");
            cprintf("  agent_type = %d\n", info.agent_type);
            cprintf("  heartbeat_interval = %d\n", info.heartbeat_interval);
            cprintf("  resource_quota = %u\n", info.resource_quota);
            cprintf("  loop_state = %d\n", info.loop_state);
            cprintf("  agent_ctx_va = 0x%lx\n", info.agent_ctx_va);
            cprintf("  ctx_total_len = %u\n", info.ctx_total_len);
            cprintf("  ctx_cur_pos = %u\n", info.ctx_cur_pos);
        } else {
            cprintf("agent_info call failed, ret=%d\n", info_ret);
        }
        int agent_exit_code;
        int wait_pid_agent = waitpid(agent_pid, &agent_exit_code);
        cprintf("agent pid %d exit, exit code = %d\n", wait_pid_agent, agent_exit_code);
    }
    cprintf("\n===== Agent Test Program All Done =====\n");
    return 0;
}
