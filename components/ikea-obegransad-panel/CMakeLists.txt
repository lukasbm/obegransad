idf_component_register(
    SRCS "ikea-obegransad-panel.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_spi esp_driver_gpio esp_driver_rmt hal freertos spi_flash esp_timer
)
