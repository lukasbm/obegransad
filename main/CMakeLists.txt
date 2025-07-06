idf_component_register(
    SRCS "device.cpp" "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client json esp_http_server button esp-wifi-connect
)
