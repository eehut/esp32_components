idf_component_register(
    SRCS "lcd_driver_spi.c" "lcd_driver_i2c.c" "lcd_display.c"
    INCLUDE_DIRS "include"
    REQUIRES driver lcd_font bus_manager uptime
)
