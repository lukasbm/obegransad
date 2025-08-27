idf_component_register(
    SRCS "scene_switcher.cpp" "weather.cpp" "device.cpp" "main.cpp" "clock.cpp" "server.cpp" "config.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client esp_http_server json button esp-wifi-connect ikea-obegransad-panel
    PRIV_REQUIRES esp-tls mbedtls nvs_flash
    EMBED_FILES "../data/bold_glyphs.bwb" "../data/cloud.bwb" "../data/firework.bwb" "../data/moon_small.bwb" "../data/rain.bwb" "../data/sun.bwb" "../data/thin_glyphs.bwb" "../data/wifi.bwb"
)
