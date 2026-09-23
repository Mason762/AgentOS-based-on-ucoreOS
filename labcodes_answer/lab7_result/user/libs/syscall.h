#ifndef __USER_LIBS_SYSCALL_H__
#define __USER_LIBS_SYSCALL_H__

#define MAX_ARGS        5
#define T_SYSCALL       0x80

// 系统调用号定义
#define SYS_agent_create    22
#define SYS_agent_info      23
#define SYS_tool_call       202
#define SYS_tool_list       203

// 前向声明
struct agent_info;
struct tool_request;

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
int tool_call(uintptr_t req_buf);
int tool_list(uint32_t *out_result_len);

#endif /* !__USER_LIBS_SYSCALL_H__ */