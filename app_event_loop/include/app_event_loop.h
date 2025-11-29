/**
 * @file app_event_loop.h
 * @brief 通用事件循环组件接口
 */
#pragma once

#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化应用事件循环
 * 
 * @param queue_size 事件队列大小
 * @param task_priority 事件处理任务优先级
 * @return esp_err_t 
 */
esp_err_t app_event_loop_init(int queue_size, int task_priority);

/**
 * @brief 注册事件处理器
 * 
 * @param event_base 事件基础（事件类别标识符）
 * @param event_id 事件ID，可以是：
 *                 - 具体的事件ID值（如 EXT_GPIO_EVENT_BUTTON_PRESSED）
 *                 - ESP_EVENT_ANY_ID：匹配该 event_base 下的所有事件
 *                 注意：不支持按位操作指定多个事件ID。如需处理多个特定事件，
 *                 可以多次调用此函数注册同一处理函数，或在处理函数内通过 switch 判断
 * @param event_handler 事件处理函数, 请注意，事件函数在事件循环中执行，请不要阻塞或长时间执行
 *                      并考虑并发任务的安全性
 * @param event_handler_arg 事件处理函数参数（传递给处理函数的 handler_args）
 * @return esp_err_t ESP_OK 成功，其他值表示错误
 */
esp_err_t app_event_handler_register(esp_event_base_t event_base,
                                    int32_t event_id,
                                    esp_event_handler_t event_handler,
                                    void *event_handler_arg);

/**
 * @brief 注销事件处理器
 * 
 * @param event_base 事件基础（事件类别标识符）
 * @param event_id 事件ID，必须与注册时使用的值一致：
 *                 - 具体的事件ID值
 *                 - ESP_EVENT_ANY_ID（如果注册时使用了该值）
 * @param event_handler 事件处理函数（必须与注册时使用的函数指针一致）
 * @return esp_err_t ESP_OK 成功，其他值表示错误
 */
esp_err_t app_event_handler_unregister(esp_event_base_t event_base,
                                      int32_t event_id,
                                      esp_event_handler_t event_handler);

/**
 * @brief 发送事件
 * 
 * @param event_base 事件基础（事件类别标识符）
 * @param event_id 事件ID，必须是具体的事件ID值（不能使用 ESP_EVENT_ANY_ID）
 * @param event_data 事件数据指针，可以为 NULL
 * @param event_data_size 事件数据大小（字节），如果 event_data 为 NULL 则应为 0
 * @param ticks_to_wait 等待时间（FreeRTOS ticks），如果队列满时等待的时间
 * @return esp_err_t ESP_OK 成功，ESP_ERR_TIMEOUT 超时，其他值表示错误
 */
esp_err_t app_event_post(esp_event_base_t event_base,
                        int32_t event_id,
                        void *event_data,
                        size_t event_data_size,
                        TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif 