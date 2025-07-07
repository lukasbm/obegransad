idf_component_register(
    SRCS "ikea-obegransad-panel.c"
    INCLUDE_DIRS "include"
    REQUIRES driver hal freertos spi_flash
)
