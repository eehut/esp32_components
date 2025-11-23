
# 时间同步封装

引入app_event_loop和uptime组件。

我需要将SNTP的操作封装成一个极其简单的API，这样才有意义。

 typedef struct TimeSyncConfig
 {
    server_url
    sync_interval; 0 -- 只同步一次  
 }time_sync_config_t;

 typedef struct TimeSyncStatus
 {
    bool synced;
    sys_tick_t synced_time; // 使用sys tick 用来计算同步时间
    char server_url[]; // 当前同步的服务器，目前只支持一个，可以直接从配置中获取
    uint32_t sync_interval; // 来自配置
 }time_sync_status_t;

 time_sync_set_config()
 time_sync_get_config()
 time_sync_get_status()
 time_sync_start()
 time_sync_stop()

引入 app_event_loop 组件，在同步成功或失败后，发送相关的event出来。

