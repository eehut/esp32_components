/**
 * @file lcd_anim.c
 * @author LiuChuansen (179712066@qq.com)
 * @brief LCD动画显示功能实现
 * @version 0.1
 * @date 2025-11-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "lcd_anim.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "lcd_anim";

/**
 * @brief 动图实例结构体
 */
struct lcd_anim_instance
{
    struct lcd_anim_schedule *schedule;  ///< 所属调度器
    uint16_t x;                          ///< 显示位置X坐标
    uint16_t y;                          ///< 显示位置Y坐标
    bool reverse;                        ///< 是否反向显示（黑底白字）
    uint16_t frame_index;                ///< 当前帧索引
    sys_tick_t next_frame_tick;          ///< 下次帧刷新时间（0表示需要立即显示）
    const lcd_anim_t *anim;              ///< 动画对象指针
    struct lcd_anim_instance *next;       ///< 下一个实例指针
};

// 动图调度器结构体定义已移至头文件 lcd_anim.h

/**
 * @brief 获取单色位图的宽度和高度
 */
static void get_mono_img_size(const void *frame, uint16_t *width, uint16_t *height)
{
    const lcd_mono_img_t *img = (const lcd_mono_img_t *)frame;
    if (img && width && height) {
        *width = img->width;
        *height = img->height;
    }
}

/**
 * @brief 初始化动画对象
 */
int lcd_anim_init(lcd_anim_t *anim, lcd_anim_type_t type, uint16_t period, const void *first_frame)
{
    if (!anim || !first_frame) {
        ESP_LOGE(TAG, "Invalid parameters for animation init");
        return -1;
    }

    if (type >= LCD_ANIM_TYPE_MAX) {
        ESP_LOGE(TAG, "Invalid animation type: %d", type);
        return -1;
    }

    // 初始化动画对象
    memset(anim, 0, sizeof(lcd_anim_t));
    anim->type = type;
    anim->frame_period = period;
    anim->frame_num = 0;

    // 根据类型获取首帧尺寸
    if (type == LCD_ANIM_TYPE_MONO_IMG) {
        get_mono_img_size(first_frame, &anim->width, &anim->height);
    } else {
        ESP_LOGE(TAG, "Unsupported animation type: %d", type);
        return -1;
    }

    // 添加首帧
    anim->head_frame.data = first_frame;
    anim->head_frame.next = NULL;
    anim->frame_num = 1;

    return 0;
}

/**
 * @brief 向动画对象添加一帧
 */
int lcd_anim_add_frame(lcd_anim_t *anim, const void *frame)
{
    if (!anim || !frame) {
        ESP_LOGE(TAG, "Invalid parameters for anim add");
        return -1;
    }

    // 验证帧尺寸是否与首帧一致
    if (anim->type == LCD_ANIM_TYPE_MONO_IMG) {
        uint16_t width, height;
        get_mono_img_size(frame, &width, &height);
        if (width != anim->width || height != anim->height) {
            ESP_LOGE(TAG, "Frame size mismatch: expected %dx%d, got %dx%d", 
                     anim->width, anim->height, width, height);
            return -1;
        }
    }

    // 找到链表末尾
    struct lcd_anim_frame *current = &anim->head_frame;
    while (current->next != NULL) {
        current = current->next;
    }

    // 创建新帧节点
    struct lcd_anim_frame *new_frame = (struct lcd_anim_frame *)malloc(sizeof(struct lcd_anim_frame));
    if (!new_frame) {
        ESP_LOGE(TAG, "Failed to allocate memory for frame");
        return -1;
    }

    new_frame->data = frame;
    new_frame->next = NULL;
    current->next = new_frame;
    anim->frame_num++;

    return 0;
}

/**
 * @brief 释放动画对象占用的内存
 */
void lcd_anim_release(lcd_anim_t *anim)
{
    if (!anim) {
        return;
    }

    // 释放除头节点外的所有帧节点
    struct lcd_anim_frame *current = anim->head_frame.next;
    while (current != NULL) {
        struct lcd_anim_frame *next = current->next;
        free(current);
        current = next;
    }

    // 清空头节点
    anim->head_frame.next = NULL;
    anim->frame_num = 0;
}

/**
 * @brief 查找指定位置的动图实例
 */
static lcd_anim_instance_t *find_instance(lcd_anim_schedule_t *schedule, uint16_t x, uint16_t y, const lcd_anim_t *anim)
{
    if (!schedule) {
        return NULL;
    }

    lcd_anim_instance_t *current = schedule->head;
    while (current != NULL) {
        if (current->x == x && current->y == y && current->anim == anim) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * @brief 获取指定索引的帧数据
 */
static const void *get_frame_data(const lcd_anim_t *anim, uint16_t frame_index)
{
    if (!anim || frame_index >= anim->frame_num) {
        return NULL;
    }

    struct lcd_anim_frame *current = (struct lcd_anim_frame *)&anim->head_frame;
    for (uint16_t i = 0; i <= frame_index; i++) {
        if (i == frame_index) {
            return current->data;
        }
        current = current->next;
        if (!current) {
            break;
        }
    }

    return NULL;
}

/**
 * @brief 播放动图（如果实例不存在则自动创建）
 */
lcd_anim_handle_t lcd_anim_play(lcd_anim_schedule_t *anim_schedule, uint16_t x, uint16_t y, const lcd_anim_t *anim, bool reverse)
{
    if (!anim_schedule || !anim) {
        ESP_LOGE(TAG, "Invalid parameters for anim play");
        return NULL;
    }

    if (!anim_schedule->disp) {
        ESP_LOGE(TAG, "Invalid display handle in schedule");
        return NULL;
    }

    // 查找是否已存在相同位置的实例
    lcd_anim_instance_t *instance = find_instance(anim_schedule, x, y, anim);
    
    // 如果不存在，创建新实例
    if (instance == NULL) {
        instance = (lcd_anim_instance_t *)malloc(sizeof(lcd_anim_instance_t));
        if (!instance) {
            ESP_LOGE(TAG, "Failed to allocate memory for anim instance");
            return NULL;
        }

        memset(instance, 0, sizeof(lcd_anim_instance_t));
        instance->schedule = anim_schedule;
        instance->x = x;
        instance->y = y;
        instance->frame_index = 0;
        instance->next_frame_tick = 0;  // 0表示需要立即显示首帧
        instance->anim = anim;
        instance->reverse = reverse;

        // 添加到链表头部
        instance->next = anim_schedule->head;
        anim_schedule->head = instance;
    }

    // 播放动画（更新当前帧）
    sys_tick_t now = uptime();
    
    // 统一处理：如果 next_frame_tick == 0 或时间已到，则显示当前帧
    if (instance->next_frame_tick == 0 || uptime_after(now, instance->next_frame_tick)) {
        // 获取当前帧数据（新实例显示首帧，已存在的实例显示下一帧）
        uint16_t display_index = instance->frame_index;
        const void *frame_data = get_frame_data(anim, display_index);
        
        if (!frame_data) {
            ESP_LOGE(TAG, "Failed to get frame data for index %d", display_index);
            return instance;  // 即使显示失败，也返回实例句柄
        }

        // 显示帧
        if (anim->type == LCD_ANIM_TYPE_MONO_IMG) {
            const lcd_mono_img_t *img = (const lcd_mono_img_t *)frame_data;
            lcd_display_mono_img(anim_schedule->disp, instance->x, instance->y, img, instance->reverse);
        }

        // 更新帧索引和下次刷新时间
        instance->frame_index = (instance->frame_index + 1) % anim->frame_num;
        instance->next_frame_tick = now + anim->frame_period;
    }

    return instance;
}

/**
 * @brief 销毁动图实例
 */
void lcd_anim_destroy(lcd_anim_handle_t anim_instance)
{
    if (!anim_instance) {
        return;
    }

    lcd_anim_instance_t *instance = (lcd_anim_instance_t *)anim_instance;
    lcd_anim_schedule_t *schedule = instance->schedule;

    if (!schedule) {
        free(instance);
        return;
    }

    // 从链表中移除
    if (schedule->head == instance) {
        schedule->head = instance->next;
    } else {
        lcd_anim_instance_t *current = schedule->head;
        while (current && current->next != instance) {
            current = current->next;
        }
        if (current) {
            current->next = instance->next;
        }
    }

    free(instance);
}

/**
 * @brief 执行动图调度一次
 */
bool lcd_anim_schedule(lcd_anim_schedule_t *anim_schedule)
{
    if (!anim_schedule) {
        return false;
    }

    bool need_update = false;
    sys_tick_t now = uptime();

    lcd_anim_instance_t *current = anim_schedule->head;
    while (current != NULL) {
        // 检查是否需要更新（next_frame_tick == 0 表示新实例需要立即显示）
        if (current->next_frame_tick == 0 || uptime_after(now, current->next_frame_tick)) {
            need_update = true;
            break;
        }
        current = current->next;
    }

    return need_update;
}

/**
 * @brief 初始化动图调度器
 */
int lcd_anim_schedule_init(lcd_anim_schedule_t *anim_schedule, uint32_t id, lcd_handle_t disp)
{
    if (!anim_schedule || !disp) {
        ESP_LOGE(TAG, "Invalid parameters for schedule init");
        return -1;
    }

    memset(anim_schedule, 0, sizeof(lcd_anim_schedule_t));
    anim_schedule->id = id;
    anim_schedule->disp = disp;
    anim_schedule->head = NULL;

    return 0;
}

/**
 * @brief 释放调度器中的所有动图实例
 */
void lcd_anim_schedule_release_all(lcd_anim_schedule_t *anim_schedule)
{
    if (!anim_schedule) {
        return;
    }

    lcd_anim_instance_t *current = anim_schedule->head;
    while (current != NULL) {
        lcd_anim_instance_t *next = current->next;
        free(current);
        current = next;
    }

    anim_schedule->head = NULL;
}

