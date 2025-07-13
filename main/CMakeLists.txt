idf_component_register(
    SRCS "weather.cpp" "device.cpp" "main.cpp" "clock.cpp" "server.cpp" "config.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client esp_http_server json button esp-wifi-connect ikea-obegransad-panel
    PRIV_REQUIRES esp-tls mbedtls
)
