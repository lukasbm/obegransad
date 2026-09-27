# All backends are always registered: the IDF build system evaluates this file
# before Kconfig, so SRCS/REQUIRES must not depend on CONFIG_*. The backend
# that is not selected compiles to an empty translation unit (guards inside
# device_*.cpp / button_*.cpp).
set(srcs "scene_switcher.cpp" "weather.cpp" "weather_client.cpp" "app_events.cpp" "status_led.cpp" "state.cpp" "popup.cpp" "main.cpp" "clock.cpp" "config_store.cpp" "app_control.cpp" "config_server.cpp" "device_hw.cpp" "device_sim.cpp" "device_host.cpp" "button_hw.cpp" "button_sim.cpp")
set(reqs esp_http_client esp_http_server cjson esp_timer ikea-obegransad-panel
         obegransad-sim obegransad-host)

idf_build_get_property(obg_target IDF_TARGET)
if(obg_target STREQUAL "linux")
    # Host build: the host network stack is used; there is no Wi-Fi, Ethernet
    # or GPIO driver to link against.
else()
    list(APPEND reqs esp-wifi-connect esp_eth esp_netif lwip esp_driver_gpio)
endif()

# Sprite blobs and their C++ wrappers are generated from
# assets/sprites/sprites.toml by assets/sprites/convert_all.sh; the generated
# file below lists the EMBED_FILES.
include("${CMAKE_CURRENT_LIST_DIR}/sprites/sprites.cmake")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES ${reqs}
    PRIV_REQUIRES esp-tls mbedtls nvs_flash
    EMBED_FILES ${OBG_SPRITE_EMBED_FILES}
)

# Set C++17 for modern features used in weather display
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

# Optimize for ESP32-C3 real-time LED refresh requirements
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os # Optimize for size
    -ffast-math # Speed up floating point math
    -fno-exceptions # Reduce code size
)
