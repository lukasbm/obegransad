idf_component_register(
    SRCS "esp_timer_shim.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer freertos
)
