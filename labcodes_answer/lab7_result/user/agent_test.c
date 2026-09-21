#include <defs.h>
#include <ulib.h>
#include <stdio.h>
#include <syscall.h>

// !!!!! 用户态结构体，必须和内核 syscall.c 的 struct agent_info 完全一致 !!!!!
struct agent_info {
    int agent_type;
    int heartbeat_interval;
    size_t resource_quota;
    int loop_state;
    uintptr_t agent_ctx_va;
    size_t ctx_total_len;
    size_t ctx_cur_pos;
} __attribute__((packed));


int main(void)
{
    char padding[4096];
    struct agent_info info;

    // 加这两行，让编译器知道变量被使用，不会优化删除！
    padding[0] = 0;
    info.agent_type = 0;

    cprintf("===== Agent Test Program Start =====\n");
    cprintf("user side agent_info size = %d\n", sizeof(struct agent_info));
    // ========== 测试原有封装函数 fork + waitpid ==========
    cprintf("\n[Test 1] Test original fork & waitpid\n");
    int pid_fork = fork();
    if (pid_fork < 0) {
        cprintf("fork failed!\n");
    } else if (pid_fork == 0) {
        // 普通子进程
        cprintf("fork child: pid = %d, I am normal process\n", getpid());
        exit(0);
    } else {
        // 父进程等待【指定pid】子进程，使用 waitpid
        int ret;
        int wait_pid = waitpid(pid_fork, &ret);
        cprintf("parent: normal child pid %d exit, ret=%d\n", wait_pid, ret);
    }

    // ========== 测试新系统调用 agent_create + agent_info ==========
    cprintf("\n[Test 2] Test new agent_create & agent_info\n");
    int agent_pid = agent_create(10, 4096);
    if (agent_pid < 0) {
        cprintf("agent_create failed!\n");
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
        // 等待Agent进程退出
        int agent_exit_code;
        int wait_pid_agent = waitpid(agent_pid, &agent_exit_code);
        cprintf("agent pid %d exit, exit code = %d\n", wait_pid_agent, agent_exit_code);
    }

    cprintf("\n===== Agent Test Program All Done =====\n");
    return 0;
}
