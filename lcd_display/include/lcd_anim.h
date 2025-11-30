#ifndef __LCD_ANIM_H__
#define __LCD_ANIM_H__

/**
 * @brief LCD动画显示功能实现
 * 
 * 提供动画对象的创建、管理和播放功能，支持在LCD屏幕上显示多帧动画。
 * 
 * @author LiuChuansen (179712066@qq.com)
 * @date 2025-11-30
 * @version 1.0
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_display.h"
#include "lcd_img.h"
#include "uptime.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @file lcd_anim.h
 * @brief LCD动画显示功能
 * 
 * 提供动画对象的创建、管理和播放功能，支持在LCD屏幕上显示多帧动画。
 */

/**
 * @brief 动画类型枚举
 */
typedef enum 
{
    LCD_ANIM_TYPE_MONO_IMG = 0,  ///< 单色位图类型
    LCD_ANIM_TYPE_MAX,
} lcd_anim_type_t;

/**
 * @brief 动画帧结构体
 */
struct lcd_anim_frame
{
    const void *data;              ///< 帧数据指针
    struct lcd_anim_frame *next;   ///< 下一帧指针
};

/**
 * @brief 动画对象结构体
 */
typedef struct
{
    lcd_anim_type_t type;          ///< 动画类型
    uint16_t width;                ///< 动画宽度（像素）
    uint16_t height;               ///< 动画高度（像素）
    uint16_t frame_period;         ///< 每帧显示时间（毫秒）
    uint16_t frame_num;            ///< 帧总数
    struct lcd_anim_frame head_frame; ///< 帧链表头节点
} lcd_anim_t;

/**
 * @brief 动图实例结构体（前向声明）
 */
struct lcd_anim_instance;

/**
 * @brief 动图调度器结构体
 */
struct lcd_anim_schedule
{
    uint32_t id;                         ///< 调度器标识（如页面ID）
    lcd_handle_t disp;                   ///< LCD显示句柄
    struct lcd_anim_instance *head;      ///< 动图实例链表头
};

/**
 * @brief 动图实例类型定义
 */
typedef struct lcd_anim_instance lcd_anim_instance_t;

/**
 * @brief 动图句柄类型定义（用于返回值）
 */
typedef void * lcd_anim_handle_t;

/**
 * @brief 动图调度器类型定义
 */
typedef struct lcd_anim_schedule lcd_anim_schedule_t;

/**
 * @brief 初始化动画对象
 * 
 * @param anim 动画对象指针
 * @param type 动画类型
 * @param period 每帧显示时间（毫秒）
 * @param first_frame 首帧数据指针（对于单色位图类型，应传入 lcd_mono_img_t*）
 * @return int 成功返回0，失败返回-1
 */
int lcd_anim_init(lcd_anim_t *anim, lcd_anim_type_t type, uint16_t period, const void *first_frame);

/**
 * @brief 向动画对象添加一帧
 * 
 * @param anim 动画对象指针
 * @param frame 帧数据指针（对于单色位图类型，应传入 lcd_mono_img_t*）
 * @return int 成功返回0，失败返回-1
 */
int lcd_anim_add_frame(lcd_anim_t *anim, const void *frame);

/**
 * @brief 释放动画对象占用的内存
 * 
 * @param anim 动画对象指针
 */
void lcd_anim_release(lcd_anim_t *anim);

/**
 * @brief 播放动图（如果实例不存在则自动创建）
 * 
 * 在指定调度器的指定位置播放动画。如果该位置不存在动画实例，则自动创建并显示首帧；
 * 如果已存在实例，则检查是否需要切换到下一帧并更新显示。
 * 
 * @param anim_schedule 动图调度器指针
 * @param x 显示位置X坐标
 * @param y 显示位置Y坐标
 * @param anim 动画对象指针
 * @param reverse 是否反向显示（黑底白字）
 * @return lcd_anim_handle_t 动画实例句柄，失败返回NULL
 */
lcd_anim_handle_t lcd_anim_play(lcd_anim_schedule_t *anim_schedule, uint16_t x, uint16_t y, const lcd_anim_t *anim, bool reverse);

/**
 * @brief 销毁动图实例
 * 
 * @param anim_instance 动图实例句柄
 */
void lcd_anim_destroy(lcd_anim_handle_t anim_instance);

/**
 * @brief 执行动图调度一次
 * 
 * 检查调度器中的所有动图实例，判断是否有需要更新的帧。
 * 
 * @param anim_schedule 动图调度器指针
 * @return bool 如果有动图需要更新返回true，否则返回false
 */
bool lcd_anim_schedule(lcd_anim_schedule_t *anim_schedule);

/**
 * @brief 初始化动图调度器
 * 
 * @param anim_schedule 动图调度器指针
 * @param id 调度器标识（如页面ID）
 * @param disp LCD显示句柄
 * @return int 成功返回0，失败返回-1
 */
int lcd_anim_schedule_init(lcd_anim_schedule_t *anim_schedule, uint32_t id, lcd_handle_t disp);

/**
 * @brief 释放调度器中的所有动图实例
 * 
 * @param anim_schedule 动图调度器指针
 */
void lcd_anim_schedule_release_all(lcd_anim_schedule_t *anim_schedule);

#ifdef __cplusplus
}
#endif

#endif // __LCD_ANIM_H__

