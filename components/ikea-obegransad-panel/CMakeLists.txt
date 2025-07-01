idf_component_register(
    SRCS "ikea-obegransad-panel.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_spi esp_driver_rmt esp_driver_gptimer freertos
)