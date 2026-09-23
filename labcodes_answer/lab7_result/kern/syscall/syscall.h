#ifndef __KERN_SYSCALL_SYSCALL_H__
#define __KERN_SYSCALL_SYSCALL_H__
struct agent_info;

void syscall(void);

#define SYS_agent_create 22
#define SYS_agent_info   23
#define SYS_tool_call     202
#define SYS_tool_list     203

// 函数声明
int agent_create(int heartbeat, size_t quota);
int agent_info(int pid, struct agent_info *buf);
#endif /* !__KERN_SYSCALL_SYSCALL_H__ */