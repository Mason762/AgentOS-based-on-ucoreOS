#include <defs.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>
#include <agent_tool.h>
#define MAX_ARGS            5
int
syscall(int num, ...) {
    va_list ap;
    va_start(ap, num);
    uint32_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i ++) {
        a[i] = va_arg(ap, uint32_t);
    }
    va_end(ap);
    asm volatile (
        "int %1;"
        : "=a" (ret)
        : "i" (T_SYSCALL),
          "a" (num),
          "d" (a[0]),
          "c" (a[1]),
          "b" (a[2]),
          "D" (a[3]),
          "S" (a[4])
        : "cc", "memory");
    return ret;
}
int
sys_exit(int error_code) {
    return syscall(SYS_exit, error_code);
}
int
sys_fork(void) {
    return syscall(SYS_fork);
}
int
sys_wait(int pid, int *store) {
    return syscall(SYS_wait, pid, store);
}
int
sys_yield(void) {
    return syscall(SYS_yield);
}
int
sys_kill(int pid) {
    return syscall(SYS_kill, pid);
}
int
sys_getpid(void) {
    return syscall(SYS_getpid);
}
int
sys_putc(int c) {
    return syscall(SYS_putc, c);
}
int
sys_pgdir(void) {
    return syscall(SYS_pgdir);
}
size_t
sys_gettime(void) {
    return syscall(SYS_gettime);
}
void
sys_lab6_set_priority(uint32_t priority)
{
    syscall(SYS_lab6_set_priority, priority);
}
int
sys_sleep(unsigned int time) {
    return syscall(SYS_sleep, time);
}
// Agent‑OS 任务一 用户态封装
int
agent_create(int heartbeat, size_t quota)
{
    return syscall(SYS_agent_create, heartbeat, quota);
}
int
agent_info(int pid, struct agent_info *buf)
{
    return syscall(SYS_agent_info, pid, buf);
}
// Agent‑OS 任务二 用户态封装
int sys_tool_call(struct tool_request *req)
{
    cprintf("[USER tool_call] =====================\n");
    cprintf("[USER tool_call] tool_name: %s\n", req->tool_name);
    cprintf("[USER tool_call] param_cnt: %u\n", req->param_cnt);
    int p;
    for(p = 0; p < req->param_cnt; p++){
        struct tool_param *prm = &req->params[p];
        cprintf("[USER tool_call]   param[%d] key=%s, type=%d, ",
               p, prm->key, prm->val_type);
        if(prm->val_type == PARAM_TYPE_INT){
            cprintf("ival=%d\n", prm->val.ival);
        }else if(prm->val_type == PARAM_TYPE_STRING){
            cprintf("sval=%s\n", prm->val.sval);
        }else{
            cprintf("unknown type\n");
        }
    }
    int ret = syscall(SYS_tool_call, (uint32_t)req, 0,0,0,0);
    cprintf("[USER tool_call] syscall return ret=%d (0x%08x)\n", ret, ret);
    cprintf("[USER tool_call] =====================\n");
    return ret;
}

int sys_tool_list(uint32_t *out_count)
{
    cprintf("[USER tool_list] call tool_list, out_count ptr=0x%08x\n", (uintptr_t)out_count);
    int ret = syscall(SYS_tool_list, (uint32_t)out_count,0,0,0,0);
    cprintf("[USER tool_list] syscall return ret=%d, total_items=%u\n", ret, *out_count);
    cprintf("[USER tool_list] =====================\n");
    return ret;
}

// Agent‑OS 任务三 用户态封装
int sys_context_push(struct context_node *node)
{
    return syscall(SYS_context_push, (uint32_t)node);
}

int sys_context_query(int idx, struct context_node *buf)
{
    return syscall(SYS_context_query, (uint32_t)idx, (uint32_t)buf);
}

int sys_context_rollback(int target_idx)
{
    return syscall(SYS_context_rollback, (uint32_t)target_idx);
}

int sys_context_clear(void)
{
    return syscall(SYS_context_clear);
}