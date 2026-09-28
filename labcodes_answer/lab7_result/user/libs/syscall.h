#ifndef __USER_LIBS_SYSCALL_H__
#define __USER_LIBS_SYSCALL_H__

#define MAX_ARGS        5
#define T_SYSCALL       0x80

// 系统调用号定义
#define SYS_agent_create    22
#define SYS_agent_info      23
#define SYS_tool_call       202
#define SYS_tool_list       203



// =========任务三新增系统调用号=========
#define SYS_context_push      24
#define SYS_context_query     25
#define SYS_context_rollback  26
#define SYS_context_clear     27

// 前向声明
struct agent_info;
struct tool_request;
struct context_node;

// syscall 只声明，实现在 syscall.c
int syscall(int num, ...);

// ========= 用户态封装函数声明 =========
int sys_exit(int error_code);
int sys_fork(void);
int sys_wait(int pid, int *store);
int sys_yield(void);
int sys_kill(int pid);
int sys_getpid(void);
int sys_putc(int c);
int sys_pgdir(void);
size_t sys_gettime(void);

int agent_create(int heartbeat, size_t quota);
int agent_info(int pid, struct agent_info *buf);

/* FOR LAB6 ONLY */
void sys_lab6_set_priority(uint32_t priority);
int sys_sleep(unsigned int time);

// 任务二封装声明
int sys_tool_call(struct tool_request *req);
int sys_tool_list(uint32_t *out_count);

// 任务三封装声明
int sys_context_push(struct context_node *node);
int sys_context_query(int idx, struct context_node *buf);
int sys_context_rollback(int target_idx);
int sys_context_clear(void);
#endif /* !__USER_LIBS_SYSCALL_H__ */