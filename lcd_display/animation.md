
# LCD的动画显示系统设计

## 基本概念 

**动画对象**

一个动画对象包含一个动画的基本信息，包含每帧显示时间，每帧图片的指针地址。一个动画至少一帧图片，首次指定的图片大小作为动画的显示大小。

动画的对象可以有多个类型，暂时支持 单色位图类型 

```c
typedef enum 
{
    LCD_ANIM_TYPE_MONO_IMG = 0,
    LCD_ANIM_TYPE_MAX,
}lcd_anim_type_t;
```

```c

struct lcd_anim_frame
{
    const void *data;
    struct lcd_anim_frame * next;
};

typedef struct
{
   lcd_anim_type_t type;
   uint16_t width;
   uint16_t height;
   uint16_t frame_period;// 每帧显示时间
   uint16_t frame_num; // 帧总数 
   struct lcd_anim_frame head_frame;
}lcd_anim_t;

```

动画对象API

```c
// 推荐使用 静态方式定义动图，只有添加图片时，才创建新的链表节点

lcd_anim_init(lcd_anim_t *anim, lcd_anim_type_t type, uint16_t period, const void *first_frame)

lcd_anim_add(lcd_anim_t *anim, const void *frame);

// 释放动图占用的内存 
lcd_anim_release(lcd_anim_t *anim);

```

**动图调度器**

如果说动图定义一个类似GIF的静态数据集，那么动图调度器，将这些GIF集中管理起来，并实现动图的显示。

一个显示屏应用，可以根据页面数量创建多个动图调度器。比如每个页有一个动图调度器，切到指定页刷新时，对应的动图调度器才执行。

为了实现一个动图可以在一个页面上多个位置上显示，与显示位置关联的对象定义为动图实例（anim_instance）

```c

struct lcd_anim_instance
{
    struct lcd_anim_schedule *schedule;
    uint16_t x;
    uint16_t y;
    uint16_t frame_index; // 当前frame 帧索引 
    sys_tick_t next_frame_tick;// 下次帧刷新时间 
    const lcd_anim_t * anim;
    struct lcd_anim_instance *next;
};

struct lcd_anim_schedule
{
    uint32_t id;// 调度器标识，有可能是页面ID等。
    lcd_handle_t disp;
    lcd_anim_instance_t *head;
};


typedef struct lcd_anim_instance lcd_anim_instance_t;

// 定义一个用于返回值的指针 
typedef void * lcd_anim_handle_t;

typedef struct lcd_anim_schedule lcd_anim_schedule_t;

```

动图调度器工作原理

在LCD的显示任务的刷新周期中，获取当前页面的动图调度器，然后检测是否有动图需要刷新页面了，
如果有，返回True，LCD任务设定页面数据dirty为true，然后调用指定页面重绘函数，在页面绘制函数中，
调用lcd_anim_play()函数写入新的动图数据。

所以总的来说，使用lcd_anim_play() 函数封装动图运行实例的具体细节。

函数声明：
```c
// 创建一个动图实例 
lcd_anim_handle_t lcd_anim_create(lcd_anim_schedule_t * anim_schedule, uint16_t x, uint16_t y, const lcd_anim_t *anim);

void lcd_anim_play(lcd_anim_handle_t anim_handle);
void lcd_anim_destory(lcd_anim_handle_t anim_handle);

bool lcd_anim_schedule(lcd_anim_schedule_t *anim_schedule); // 执行调度一次，返回是否有动图是否要更新 
void lcd_anim_schedule_release_all(lcd_anim_schedule_t *anim_schedule); // 释放所有动图
```

lcd_anim_create() 函数的实现细节：

- 首先在调度器检查是否存在 （x,y,anim）的动图实例，如果不存在则创建一个并初始化，如果已存在则返回对象已存在。

lcd_anim_play() 函数的实现细节：
- 如果是新创建实例，将首帧图片写入绑定的显存中。
- 如果是旧的实例，检查下一帧显示周期是否到了，如果到了，获取下一帧数据，并写入到显存中。

lcd_anim_destroy() 用于删除一个动图实例 



