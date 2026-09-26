# All backends are always registered: the IDF build system evaluates this file
# before Kconfig, so SRCS/REQUIRES must not depend on CONFIG_*. The backend
# that is not selected compiles to an empty translation unit (guards inside
# device_hw.cpp / device_sim.cpp / button_hw.cpp / button_sim.cpp).
set(srcs "scene_switcher.cpp" "weather.cpp" "weather_client.cpp" "app_events.cpp" "status_led.cpp" "state.cpp" "popup.cpp" "main.cpp" "clock.cpp" "device_hw.cpp" "device_sim.cpp" "button_hw.cpp" "button_sim.cpp")
set(reqs esp_http_client cjson esp_driver_gpio esp_timer ikea-obegransad-panel
         esp-wifi-connect esp_eth esp_netif lwip obegransad-sim)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES ${reqs}
    PRIV_REQUIRES esp-tls mbedtls nvs_flash
    EMBED_FILES "../data/bold_glyphs.bwb" "../data/cloud.bwb" "../data/firework.bwb" "../data/heart.bwb" "../data/moon_small.bwb" "../data/rain.bwb" "../data/sun.bwb" "../data/thin_glyphs.bwb" "../data/wifi.bwb"
)

# Set C++17 for modern features used in weather display
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

# Optimize for ESP32-C3 real-time LED refresh requirements
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os # Optimize for size
    -ffast-math # Speed up floating point math
    -fno-exceptions # Reduce code size
)
