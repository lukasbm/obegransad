idf_component_register(
    SRCS "scene_switcher.cpp" "weather.cpp" "weather_client.cpp" "device.cpp" "app_events.cpp" "status_led.cpp" "state.cpp" "main.cpp" "clock.cpp" "server.cpp" "config.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client esp_http_server cjson button esp-wifi-connect ikea-obegransad-panel
    PRIV_REQUIRES esp-tls mbedtls nvs_flash
    EMBED_FILES "../data/index.html" "../data/bold_glyphs.bwb" "../data/cloud.bwb" "../data/firework.bwb" "../data/heart.bwb" "../data/moon_small.bwb" "../data/rain.bwb" "../data/sun.bwb" "../data/thin_glyphs.bwb" "../data/wifi.bwb"
)

# Set C++17 for modern features used in weather display
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

# Optimize for ESP32-C3 real-time LED refresh requirements
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os # Optimize for size
    -ffast-math # Speed up floating point math
    -fno-exceptions # Reduce code size
)
