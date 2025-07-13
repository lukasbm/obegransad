idf_component_register(
    SRCS "weather.cpp" "device.cpp" "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client esp_http_server json button esp-wifi-connect ikea-obegransad-panel
)
# TODO: move some stuff priv_requires?
