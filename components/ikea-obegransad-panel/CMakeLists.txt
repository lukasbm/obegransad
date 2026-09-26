# Both backends are always registered: the IDF build system evaluates this
# file before Kconfig, so REQUIRES/SRCS must not depend on CONFIG_*. The
# backend that is not selected compiles to an empty translation unit (see the
# #if guards in panel_hw.c / panel_sim.c).
idf_component_register(
    SRCS "panel_core.c" "panel_hw.c" "panel_sim.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES freertos esp_timer esp_driver_spi esp_driver_gpio esp_driver_rmt
             hal spi_flash obegransad-sim
)
