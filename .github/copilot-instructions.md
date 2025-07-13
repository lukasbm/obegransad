# IKEA Obegränsad ESP-IDF Project Instructions

## Project Overview
This is an ESP32-C3 project that drives an IKEA Obegränsad 16x16 LED panel for displaying weather information. The project uses ESP-IDF v5.4.2 and demonstrates sophisticated embedded patterns including hardware-accelerated LED control and secure HTTPS API integration.

## Architecture & Components

### Core Components
- **`components/ikea-obegransad-panel/`** - Custom C component for LED panel control using SPI+RMT+ESP Timer
- **`main/`** - C++ application logic with weather fetching, device management, and web server
- **Key managed components**: `esp-wifi-connect`, `button` for WiFi captive portal and input handling

### Hardware Architecture (Critical for Understanding)
The LED panel driver implements **Bit Code Modulation (BCM)** with precise timing:
- **SPI**: High-speed data transfer to shift registers (`spi_device_polling_transmit`)
- **RMT**: Microsecond-precision OE (Output Enable) timing control (`rmt_send_oe_pulse`)
- **ESP Timer**: 500Hz refresh with `ESP_TIMER_TASK` dispatch (NOT ISR) for safe blocking operations
- **Direct GPIO**: Fast latch pulses using `GPIO.out_w1ts.val` register access

### Data Flow Pattern
1. Weather API (HTTPS) → JSON parsing → `WeatherData` struct
2. `WeatherData` → Display rendering → LED panel via BCM timing
3. Button events → State machine → Display updates

## Critical Development Patterns

### C/C++ Mixed Language Convention
- **C components**: LED driver in `components/` uses pure C with `extern "C"` blocks for C++ compatibility
- **C++ application**: `main/` uses C++ with ESP-IDF C APIs via proper declarations
- **HTTPS requires**: `extern "C" { esp_err_t esp_crt_bundle_attach(void *conf); }` forward declaration

### ESP-IDF Component Dependencies
```cmake
# main/CMakeLists.txt - Use REQUIRES vs PRIV_REQUIRES correctly
REQUIRES ikea-obegransad-panel        # Public API exposed
PRIV_REQUIRES esp-tls mbedtls        # Internal implementation only
```

### HTTPS Pattern (Essential for API calls)
```cpp
// Required for secure OpenMeteo API calls
cfg.crt_bundle_attach = esp_crt_bundle_attach;  // Use cert bundle
cfg.use_global_ca_store = false;                // Not global store
cfg.event_handler = event_handler;              // Handle chunked responses
```

### LED Panel Timing Critical Sections
- **IRAM functions**: Only `latch_pulse()` and `oe_disable()` need `IRAM_ATTR` (direct GPIO)
- **Timer context**: Display refresh runs in ESP Timer task, NOT ISR (can use blocking SPI/RMT calls)
- **BCM timing**: `PLANE0_ON_US=100, PLANE1_ON_US=200, PLANE2_ON_US=400, PLANE3_ON_US=800` for 4-bit depth

## Development Workflow

### Build Commands
```bash
idf.py build                    # Standard build
idf.py flash monitor           # Flash and start serial monitor
idf.py menuconfig              # Configure via GUI
idf.py clean                   # Clean build artifacts
```

### Configuration Management
- **`sdkconfig.defaults`**: Committed defaults (ESP32-C3 target, HTTPS certs, log levels)
- **`sdkconfig`**: Generated file (gitignored), contains full config
- **Certificate bundle**: Enabled via `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`

### Debugging Patterns
- **Component logs**: Use `esp_log_level_set("panel", ESP_LOG_DEBUG)` for LED driver details
- **HTTP debugging**: Event handler shows chunked transfer and TLS handshake details
- **Hardware issues**: Check SPI clock speeds (2MHz default), GPIO pin assignments in `main.cpp`

## Integration Points

### External Dependencies
- **OpenMeteo API**: Weather data via HTTPS with chunked encoding support
- **WiFi Management**: `esp-wifi-connect` provides captive portal for credentials
- **Hardware**: ESP32-C3 GPIO pins hardcoded: latch=3, clk=4, di=5, oe=6, button=20

### Cross-Component Communication
- **Panel API**: Thread-safe via local response buffers, no global state in HTTP handlers
- **State machine**: `advance_state_machine()` stub for future button/WiFi event handling
- **Memory management**: Caller-owned memory pattern (e.g., `fetch_weather` allocates, caller frees)

## Project-Specific Conventions

### Error Handling
- Use `ESP_RETURN_ON_ERROR(func(), TAG, "message")` consistently
- HTTP responses use structured `response_data_t` for chunked data accumulation
- LED driver validates bounds before GPIO operations

### Memory & Performance
- LED framebuffer: Fixed `Brightness g_framebuffer[16][16]` array for real-time access
- HTTP responses: 2KB buffer limit with overflow protection
- HTTPS: Certificate bundle in flash (not RAM) for memory efficiency

When modifying this codebase, understand that timing precision and memory efficiency are critical due to the real-time LED refresh requirements and embedded constraints.
