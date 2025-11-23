idf_component_register(SRCS "time_sync.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash lwip uptime app_event_loop)

