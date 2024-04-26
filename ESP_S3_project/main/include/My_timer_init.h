#ifndef _MY_TIMER_INIT_H_
#define _MY_TIMER_INIT_H_

#define HTTPS_REQUEST_TIMER 1

enum
{
    MAIN_TASK = 1,
    MY_HTTPS_REQUEST_TASK,
    SNTP_SYNC,
    MCU_DEEP_SLEEP,
    CHANGE_LOG_PORT,
};

/**
 * @brief 返回定时器名称函数
 * @param My_task_mode - 选择定时器,
 *  MAIN_TASK: 主任务句柄,
 *  MY_HTTPS_REQUEST_TASK: HTTP_request任务句柄,
 *  SNTP_SYNC: SNTP同步定时器句柄,
 *  MCU_DEEP_SLEEP: MCU深度睡眠定时器句柄,
 *
 * @return esp_timer_handle_t
 */
char *My_timer_mode_str(int My_task_mode);

/**
 * @brief 自定义的定时器选择函数
 * @param My_task_mode - 选择定时器,
 *  MAIN_TASK: 主任务句柄,
 *  MY_HTTPS_REQUEST_TASK: HTTP_request任务句柄,
 *  SNTP_SYNC: SNTP同步定时器句柄,
 *  MCU_DEEP_SLEEP: MCU深度睡眠定时器句柄,
 *
 * @return esp_timer_handle_t
 */
esp_timer_handle_t My_timer_select(int My_task_mode);
void My_timer_callback(void *arg);
void https_request_timer_callback(void *arg);
void My_timer_init();

/**
 * @brief 自定义的定时器删除函数
 * @param My_task_mode - 选择要删除的定时器,
 *  MAIN_TASK: 主任务,
 *  MY_HTTPS_REQUEST_TASK: HTTP_request任务,
 *  SNTP_SYNC: SNTP同步定时器,
 *  MCU_DEEP_SLEEP: MCU深度睡眠定时器,
 *
 * @return void
 */
void My_timer_delete(int My_task_mode);

/**
 * @brief 自定义的定时器停止函数
 * @param My_task_mode - 选择要停止的定时器,
 *  MAIN_TASK: 主任务,
 *  MY_HTTPS_REQUEST_TASK: HTTP_request任务,
 *  SNTP_SYNC: SNTP同步定时器,
 *  MCU_DEEP_SLEEP: MCU深度睡眠定时器,
 *
 * @return void
 */
void My_timer_stop(int My_task_mode);
#endif