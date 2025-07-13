idf_component_register(
    SRCS "weather.cpp" "device.cpp" "main.cpp" "clock.cpp" "server.cpp" "config.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client esp_http_server json button esp-wifi-connect ikea-obegransad-panel
    PRIV_REQUIRES esp-tls mbedtls
)

# Note: you must have a partition named the first argument (here it's "littlefs")
# in your partition table csv file.
# https://github.com/espressif/esp-idf/tree/v5.4.2/examples/storage/littlefs
littlefs_create_partition_image(storage ../flash_data FLASH_IN_PROJECT)
