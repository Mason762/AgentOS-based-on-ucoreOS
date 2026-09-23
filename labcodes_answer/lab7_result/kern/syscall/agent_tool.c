#include <defs.h>
#include <agent_tool.h>
#include <proc.h>
#include <pmm.h>
#include <string.h>
extern size_t npage;


/*
 * tool_query_process: 查询进程信息
 * 参数可选：type(int)  0普通进程，1Agent进程；不传返回全部
 * 输出：tool_response_hdr + 若干 proc_result_item
 */
int tool_query_process(struct tool_request *req, void *out_buf, size_t max_out, size_t *out_len)
{
    *out_len = 0;
    struct tool_response_hdr *hdr = (struct tool_response_hdr *)out_buf;
    hdr->status_code = 0;
    hdr->item_count = 0;
    *out_len += sizeof(struct tool_response_hdr);
    int have_filter = 0;
    int filter_type = -1;
    uint32_t i;
    for(i = 0; i < req->param_cnt; i++){
        struct tool_param *p = &req->params[i];
        if(strcmp(p->key, "type") == 0){
            if(p->val_type != PARAM_TYPE_INT){
                hdr->status_code = -E_INVAL;
                return -E_INVAL;
            }
            filter_type = p->val.ival;
            have_filter = 1;
            if(filter_type < 0 || filter_type > 1){
                hdr->status_code = -E_INVAL;
                return -E_INVAL;
            }
        }
    }
    (void)have_filter; // 消除unused警告
    //spin_lock(&proc_list_lock);
    /* ======== 临时注释真实链表遍历，暂时返回空结果 ========
    struct proc_struct *p;
    list_for_each_entry(p, &proc_list, list_link)
    {
        if(have_filter){
            if(filter_type == AGENT_TYPE_AGENT && p->agent_type == AGENT_TYPE_NORMAL) continue;
            if(filter_type == AGENT_TYPE_NORMAL && p->agent_type == AGENT_TYPE_AGENT) continue;
        }
        if((*out_len + sizeof(struct proc_result_item)) > max_out){
            spin_unlock(&proc_list_lock);
            hdr->status_code = -E_NOMEM;
            return -E_NOMEM;
        }
        struct proc_result_item *item = (struct proc_result_item *)((char *)out_buf + *out_len);
        item->pid = p->pid;
        item->agent_type = p->agent_type;
        item->state = p->state;
        // PCB name长度50，协议只存16字节，截断拷贝
        strncpy(item->name, p->name, sizeof(item->name)-1);
        item->name[sizeof(item->name)-1] = '\0';
        hdr->item_count ++;
        *out_len += sizeof(struct proc_result_item);
    }
    spin_unlock(&proc_list_lock);
    */
    return 0;
}

/*
 * tool_get_system_status：获取系统状态，无参数
 * 输出 tool_response_hdr + sys_status_item
 */
int tool_get_system_status(struct tool_request *req, void *out_buf, size_t max_out, size_t *out_len)
{
    *out_len = 0;
    struct tool_response_hdr *hdr = (struct tool_response_hdr *)out_buf;
    hdr->status_code = 0;
    hdr->item_count = 0;
    *out_len += sizeof(struct tool_response_hdr);
    if(req->param_cnt != 0){
        hdr->status_code = -E_INVAL;
        return -E_INVAL;
    }
    if( (*out_len + sizeof(struct sys_status_item)) > max_out ){
        hdr->status_code = -E_NOMEM;
        return -E_NOMEM;
    }
    struct sys_status_item *st = (struct sys_status_item *)((char *)out_buf + *out_len);
    st->total_mem = npage * PGSIZE;
    st->free_mem = nr_free_pages() * PGSIZE;
    uint32_t proc_cnt = 0;
    //spin_lock(&proc_list_lock);
    /* ======== 临时注释真实链表遍历 ========
    struct proc_struct *p;
    list_for_each_entry(p, &proc_list, list_link){
        proc_cnt ++;
    }
    spin_unlock(&proc_list_lock);
    */
    st->proc_count = proc_cnt;
    hdr->item_count = 1;
    *out_len += sizeof(struct sys_status_item);
    return 0;
}

/*
 * tool_send_message：向目标Agent发送消息
 * 参数：pid(int), msg(string)
 * 使用uCore自带 find_proc() 查找pid
 */
int tool_send_message(struct tool_request *req, void *out_buf, size_t max_out, size_t *out_len)
{
    *out_len = 0;
    struct tool_response_hdr *hdr = (struct tool_response_hdr *)out_buf;
    hdr->status_code = 0;
    hdr->item_count = 0;
    *out_len += sizeof(struct tool_response_hdr);
    int have_pid = 0;
    int32_t target_pid = 0;
    int have_msg = 0;
    char msg_buf[PARAM_STR_VAL_MAX] = {0};
    uint32_t i;
    for(i = 0; i < req->param_cnt; i++){
        struct tool_param *par = &req->params[i];
        if(strcmp(par->key, "pid") == 0){
            if(par->val_type != PARAM_TYPE_INT){
                hdr->status_code = -E_INVAL;
                return -E_INVAL;
            }
            target_pid = par->val.ival;
            have_pid = 1;
        }else if(strcmp(par->key, "msg") == 0){
            if(par->val_type != PARAM_TYPE_STRING){
                hdr->status_code = -E_INVAL;
                return -E_INVAL;
            }
            strncpy(msg_buf, par->val.sval, sizeof(msg_buf)-1);
            msg_buf[sizeof(msg_buf)-1] = '\0';
            have_msg = 1;
        }
    }
    if(!have_pid || !have_msg){
        hdr->status_code = -E_INVAL;
        return -E_INVAL;
    }
    // uCore自带find_proc，内部会处理锁
    struct proc_struct *target_proc = find_proc(target_pid);
    if(target_proc == NULL){
        hdr->status_code = -E_NOENT;
        return -E_NOENT;
    }
    // 必须是Agent进程
    if(target_proc->agent_type != AGENT_TYPE_AGENT){
        hdr->status_code = -E_INVAL;
        return -E_INVAL;
    }
    //spin_lock(&proc_list_lock);
    /* ======== 临时注释消息写入逻辑 ========
    if( target_proc->agent_msg_len + strlen(msg_buf) + 1 >= AGENT_MSG_BUF_LEN ){
        spin_unlock(&proc_list_lock);
        hdr->status_code = -E_NOMEM;
        return -E_NOMEM;
    }
    strcpy( target_proc->agent_msg_buf + target_proc->agent_msg_len, msg_buf );
    target_proc->agent_msg_len += (strlen(msg_buf)+1);
    spin_unlock(&proc_list_lock);
    */
    //任务五扩展点：唤醒等待消息的Agent
    return 0;
}