#ifndef __AGENT_TOOL_H__
#define __AGENT_TOOL_H__
/*Byte偏移        字段                 大小(Byte)
0‑31             tool_name[32]         32
32‑35            param_cnt(uint32_t)   4
36‑84            params[0] (tool_param)49
85‑133           params[1]             49*/
/*Byte 偏移（相对于该 params [i] 起始）     字段                  大小
        0‑15 key                 [16] 参数名           16B
        16        val_type 类型标记           1B
        17‑48     val 联合体（int 或字符串） 32B*/
// ========== 协议宏定义 ==========
#define TOOL_NAME_MAX       32
#define PARAM_KEY_MAX       16
#define PARAM_STR_VAL_MAX   32
#define PARAM_MAX_COUNT     8
#define PARAM_TYPE_INT      0
#define PARAM_TYPE_STRING   1

// ========== 统一错误码（内核+用户态都可见） ==========
#ifndef E_INVAL
#define E_INVAL     22
#endif
#ifndef E_NOMEM
#define E_NOMEM     12
#endif
#ifndef E_NOENT
#define E_NOENT     2
#endif
#ifndef E_PERM
#define E_PERM      1
#endif

// ========== 请求协议：tool_request ==========
struct tool_param {
    char key[PARAM_KEY_MAX];
    uint8_t val_type;
    union {
        int32_t ival;
        char sval[PARAM_STR_VAL_MAX];
    } val;
} __attribute__((packed));

struct tool_request {
    char tool_name[TOOL_NAME_MAX];
    uint32_t param_cnt;
    struct tool_param params[PARAM_MAX_COUNT];
} __attribute__((packed));

// ========== 响应头部协议 ==========
struct tool_response_hdr {
    int32_t status_code;
    uint32_t item_count;
} __attribute__((packed));

// -------- 各个工具输出的item结构体 --------
// query_process 返回条目
struct proc_result_item {
    int32_t pid;
    int32_t agent_type;
    int32_t state;
    char name[16];
} __attribute__((packed));

// get_system_status 返回条目
struct sys_status_item {
    size_t total_mem;
    size_t free_mem;
    uint32_t proc_count;
} __attribute__((packed));

// sys_tool_list 返回的工具列表条目
struct tool_list_item {
    char name[TOOL_NAME_MAX];
    char desc[64];
} __attribute__((packed));

// ========= 新增：三个工具函数声明 =========
int tool_query_process(struct tool_request *req, void *out_buf, size_t max_out, size_t *out_len);
int tool_get_system_status(struct tool_request *req, void *out_buf, size_t max_out, size_t *out_len);
int tool_send_message(struct tool_request *req, void *out_buf, size_t max_out, size_t *out_len);

// ========== 内核工具分发表类型（仅内核使用） ==========
#ifdef __KERNEL__
typedef int (*tool_handler_t)(struct tool_request *req,
                              void *out_buf,
                              size_t max_out,
                              size_t *out_len);
struct tool_entry {
    const char *tool_name;
    tool_handler_t handler;
    const char *desc;
};
#endif
#endif