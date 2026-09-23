#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>
#include <skew_heap.h>


// process's state in his life cycle
enum proc_state {
    PROC_UNINIT = 0,  // uninitialized
    PROC_SLEEPING,    // sleeping
    PROC_RUNNABLE,    // runnable(maybe running)
    PROC_ZOMBIE,      // almost dead, and wait parent proc to reclaim his resource
};

// Saved registers for kernel context switches.
// Don't need to save all the %fs etc. segment registers,
// because they are constant across kernel contexts.
// Save all the regular registers so we don't need to care
// which are caller save, but not the return register %eax.
// (Not saving %eax just simplifies the switching code.)
// The layout of context must match code in switch.S.
struct context {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};

#define PROC_NAME_LEN               50
#define MAX_PROCESS                 4096
#define MAX_PID                     (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct proc_struct {
    enum proc_state state;                      // Process state
    int pid;                                    // Process ID
    int runs;                                   // the running times of Proces
    uintptr_t kstack;                           // Process kernel stack
    volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;                 // the parent process
    struct mm_struct *mm;                       // Process's memory management field
    struct context context;                     // Switch here to run process
    struct trapframe *tf;                       // Trap frame for current interrupt
    uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
    uint32_t flags;                             // Process flag
    char name[PROC_NAME_LEN + 1];               // Process name
    list_entry_t list_link;                     // Process link list 
    list_entry_t hash_link;                     // Process hash list
    int exit_code;                              // exit code (be sent to parent proc)
    uint32_t wait_state;                        // waiting state
    struct proc_struct *cptr, *yptr, *optr;     // relations between processes
    struct run_queue *rq;                       // running queue contains Process
    list_entry_t run_link;                      // the entry linked in run queue
    int time_slice;                             // time slice for occupying the CPU
    skew_heap_entry_t lab6_run_pool;            // FOR LAB6 ONLY: the entry in the run pool
    uint32_t lab6_stride;                       // FOR LAB6 ONLY: the current stride of the process 
    uint32_t lab6_priority;                     // FOR LAB6 ONLY: the priority of process, set by lab6_set_priority(uint32_t)

    /* ========= Agent‑OS 任务一扩展【追加在最后】 ========= */
    enum { AGENT_TYPE_NORMAL = 0, AGENT_TYPE_AGENT = 1 } agent_type; //普通进程 / Agent进程
    int heartbeat_interval;         //心跳周期(tick)，0=关闭心跳
    size_t resource_quota;          //Agent上下文内存配额（字节）
    enum { AGENT_LOOP_IDLE=0, AGENT_LOOP_THINK=1, AGENT_LOOP_WAIT=2 } loop_state; //Agent loop状态

    // 上下文路径元信息：内核仅保存元，真实数据放在用户态Agent Context区
    struct {
        size_t total_len;           // 当前上下文路径总占用字节
        size_t cur_pos;             // 当前节点位置
        size_t quota;               // 配额副本，和resource_quota保持一致
    } context_path_meta;

    uintptr_t agent_ctx_va;         // Agent Context区 用户虚拟地址；普通进程 = 0
    
    //=====任务二 send_message 简单内核消息缓冲区=====
    #define AGENT_MSG_BUF_LEN 128
    char agent_msg_buf[AGENT_MSG_BUF_LEN];
    int  agent_msg_len;
    //结束
};

#define PF_EXITING                  0x00000001      // getting shutdown

#define WT_INTERRUPTED               0x80000000                    // the wait state could be interrupted
#define WT_CHILD                    (0x00000001 | WT_INTERRUPTED)  // wait child process
#define WT_KSEM                      0x00000100                    // wait kernel semaphore
#define WT_TIMER                    (0x00000002 | WT_INTERRUPTED)  // wait timer

#define le2proc(le, member)         \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);
int do_yield(void);
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size);
int do_wait(int pid, int *code_store);
int do_kill(int pid);
//FOR LAB6, set the process's priority (bigger value will get more CPU time) 
void lab6_set_priority(uint32_t priority);
int do_sleep(unsigned int time);
#endif /* !__KERN_PROCESS_PROC_H__ */

