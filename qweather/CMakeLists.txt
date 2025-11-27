
idf_component_register(
    SRCS qweather.c
    INCLUDE_DIRS . include
    REQUIRES app_event_loop esp_http_client json mbedtls tweetnacl
    PRIV_REQUIRES espressif__zlib
)