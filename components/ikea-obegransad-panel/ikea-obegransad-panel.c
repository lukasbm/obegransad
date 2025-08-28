#include "ikea-obegransad-panel.h"

#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "soc/gpio_struct.h"
#include <math.h>
#include <string.h>

// Timing constants for BCM (Bit Code Modulation)
#define FRAME_PERIOD_US 2000 // 500Hz refresh rate = 2000µs period total
//  make sure that the sum of all plane times is less than
// FRAME_PERIOD_US - (some buffer for spi, scheduler, etc. overhead)

// color depth settings
#define BIT_DEPTH 4 // has to be between 1 and 8
#define PLANE0_ON_US 50
#define PLANE1_ON_US 100
#define PLANE2_ON_US 200
#define PLANE3_ON_US 400

static const char *TAG = "panel";

const Brightness brightness_levels[] = {PANEL_BRIGHTNESS_OFF,
                                        PANEL_BRIGHTNESS_1, PANEL_BRIGHTNESS_2,
                                        PANEL_BRIGHTNESS_3};

// Each bitplane requires 32 bytes (256 LEDs / 8 bits per byte)
#define BITPLANE_SIZE_BYTES ((PANEL_WIDTH * PANEL_HEIGHT) / 8)

// LUT for OBEGRÄNSAD panel wiring
static const uint8_t lut[16][16] = {
    {23, 22, 21, 20, 19, 18, 17, 16, 7, 6, 5, 4, 3, 2, 1, 0},
    {24, 25, 26, 27, 28, 29, 30, 31, 8, 9, 10, 11, 12, 13, 14, 15},
    {39, 38, 37, 36, 35, 34, 33, 32, 55, 54, 53, 52, 51, 50, 49, 48},
    {40, 41, 42, 43, 44, 45, 46, 47, 56, 57, 58, 59, 60, 61, 62, 63},
    {87, 86, 85, 84, 83, 82, 81, 80, 71, 70, 69, 68, 67, 66, 65, 64},
    {88, 89, 90, 91, 92, 93, 94, 95, 72, 73, 74, 75, 76, 77, 78, 79},
    {103, 102, 101, 100, 99, 98, 97, 96, 119, 118, 117, 116, 115, 114, 113,
     112},
    {104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126,
     127},
    {151, 150, 149, 148, 147, 146, 145, 144, 135, 134, 133, 132, 131, 130, 129,
     128},
    {152, 153, 154, 155, 156, 157, 158, 159, 136, 137, 138, 139, 140, 141, 142,
     143},
    {167, 166, 165, 164, 163, 162, 161, 160, 183, 182, 181, 180, 179, 178, 177,
     176},
    {168, 169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190,
     191},
    {215, 214, 213, 212, 211, 210, 209, 208, 199, 198, 197, 196, 195, 194, 193,
     192},
    {216, 217, 218, 219, 220, 221, 222, 223, 200, 201, 202, 203, 204, 205, 206,
     207},
    {231, 230, 229, 228, 227, 226, 225, 224, 247, 246, 245, 244, 243, 242, 241,
     240},
    {232, 233, 234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254,
     255}};

// Driver state - hardware handles and configuration
static uint8_t g_brightness_panel = 255;
static uint8_t g_brightness_user = 255;
static panel_config_t g_config;
static spi_device_handle_t g_spi;
static rmt_channel_handle_t g_rmt_oe;
static rmt_encoder_handle_t g_rmt_encoder;
static esp_timer_handle_t g_refresh_timer; // ESP Timer for precise 500Hz timing
static uint8_t g_plane_idx = 0; // Current bit plane index for display

// Buffers
static uint8_t g_framebuffer[PANEL_HEIGHT][PANEL_WIDTH];
static uint8_t g_bitplanes[BIT_DEPTH][BITPLANE_SIZE_BYTES];
static volatile bool g_refresh_needed = false;

// Timing in microseconds
static const uint32_t plane_times_us[BIT_DEPTH] = {PLANE0_ON_US, PLANE1_ON_US,
                                                   PLANE2_ON_US, PLANE3_ON_US};

// Forward declarations
static void refresh_timer_callback(void *arg);
static void prepare_bitplane(uint8_t plane);
static void display_bitplane(uint8_t plane);
static void print_bitplane(uint8_t plane);
static void print_framebuffer(void);

// Initialization helper functions for modular setup
static esp_err_t init_gpio_pins(void);
static esp_err_t init_spi_interface(void);
static esp_err_t init_refresh_timer(void);

// RMT helper functions for precise OE (Output Enable) timing control
static esp_err_t rmt_setup_oe_channel(void);
static void rmt_send_oe_pulse(uint32_t duration_us);

// Latch pulse: High->Low transition to capture shift register data into output
static inline void IRAM_ATTR latch_pulse(void) {
  // Ensure setup time after SPI clock stops
  esp_rom_delay_us(1); // 1µs setup time
  // Latch pulse: High for sufficient duration
  GPIO.out_w1ts.val = (1 << g_config.latch_pin); // Latch high
  esp_rom_delay_us(2); // 2µs latch pulse width - critical for reliable capture
  GPIO.out_w1tc.val = (1 << g_config.latch_pin); // Latch low
  // Hold time before next operation
  esp_rom_delay_us(1); // 1µs hold time
}

//////////////////////////////////
// important API functions //
//////////////////////////////////

esp_err_t panel_init(panel_config_t config) {
  ESP_LOGI(TAG, "Initializing IKEA Obegränsad panel driver");
  g_config = config;

  // Initialize framebuffer and bitplanes to zero (all LEDs off)
  memset(g_framebuffer, 0, sizeof(g_framebuffer));
  memset(g_bitplanes, 0, sizeof(g_bitplanes));

  // Initialize all hardware subsystems in sequence
  ESP_RETURN_ON_ERROR(init_gpio_pins(), TAG, "GPIO initialization failed");
  ESP_RETURN_ON_ERROR(rmt_setup_oe_channel(), TAG, "RMT setup failed");
  ESP_RETURN_ON_ERROR(init_spi_interface(), TAG, "SPI initialization failed");
  ESP_RETURN_ON_ERROR(init_refresh_timer(), TAG, "Timer initialization failed");

  ESP_LOGI(TAG, "Panel driver initialized successfully - ready for refresh");
  return ESP_OK;
}

/**
 * @brief Start the ESP timer for 500Hz display refresh
 * Must be called after panel_init() to begin automatic display updates
 */
esp_err_t panel_timer_start(void) {
  return esp_timer_start_periodic(g_refresh_timer, FRAME_PERIOD_US);
}

/**
 * @brief Stop the ESP timer to halt display refresh
 * LEDs will remain in their current state until timer is restarted
 */
esp_err_t panel_timer_stop(void) { return esp_timer_stop(g_refresh_timer); }

///////////////////////////////
// Framebuffer manipulation API //
///////////////////////////////

static uint8_t map_value_generic(uint8_t value, uint8_t in_min, uint8_t in_max,
                                 uint8_t out_min, uint8_t out_max) {
  return (uint8_t)((value - in_min) * (out_max - out_min) / (in_max - in_min) +
                   out_min);
}

/**
 * @brief Maps a full 8 bit value (0-255) to the nearest supported brightness
 * level based on BIT_DEPTH
 */
static uint8_t map_value(uint8_t value) {
  if (value == 0) {
    return 0;
  } else if (value <= PANEL_BRIGHTNESS_1) {
    return PANEL_BRIGHTNESS_1;
  } else if (value <= PANEL_BRIGHTNESS_2) {
    return PANEL_BRIGHTNESS_2;
  } else {
    return PANEL_BRIGHTNESS_3;
  }
}

/**
 * @brief Set individual pixel brightness using logical coordinates
 * @param row Pixel row (0-15)
 * @param col Pixel column (0-15)
 * @param brightness Brightness level (see Brightness enum)
 */
void panel_setPixel(uint8_t row, uint8_t col, uint8_t brightness) {
  if (row >= PANEL_HEIGHT || col >= PANEL_WIDTH)
    return;
  g_framebuffer[row][col] = map_value(brightness);
  g_refresh_needed = true;
}

/**
 * @brief Fill entire panel with uniform brightness
 * @param brightness Brightness level (see Brightness enum)
 */
void panel_fill(uint8_t brightness) {
  memset(g_framebuffer, map_value(brightness), sizeof(g_framebuffer));
  g_refresh_needed = true;
}

/**
 * @brief Mark framebuffer as needing refresh (call after direct framebuffer
 * changes)
 */
void panel_commit(void) { g_refresh_needed = true; }

/**
 * @brief Set global brightness scaling factor
 * @param brightness Global brightness (0-255), applied to all pixels
 */
void panel_set_global_brightness(uint8_t brightness) {
  g_brightness_user = brightness;

  // Apply gamma correction
  float brightness_f = (float)brightness / 255.0f;
  brightness = (uint8_t)(pow(brightness_f, g_config.gamma) * 255.0f +
                         0.1f); // +0.5f for rounding
  // Clamp to valid range
  if (brightness > 255)
    brightness = 255;

  if (brightness < 0)
    brightness = 0; // Ensure non-negative

  g_brightness_panel = brightness;
  g_refresh_needed = true; // Trigger refresh with new brightness
}

/**
 * @brief Get current global brightness scaling factor
 * @return Current global brightness (0-255)
 */
uint8_t panel_get_global_brightness(void) { return g_brightness_user; }

//////////////////////////
// Actual Driver functions //
//////////////////////////

/**
 * @brief ESP Timer callback - triggers display refresh
 * This callback runs in ESP Timer task context and can safely do blocking
 * operations
 * @param arg User-defined argument (unused)
 */
static void refresh_timer_callback(void *arg) {
  // Prepare bitplanes from framebuffer if changes were made
  if (g_refresh_needed) {
    // print_framebuffer();
    for (int i = 0; i < BIT_DEPTH; i++) {
      prepare_bitplane(i);
      // print_bitplane(i); // Print each prepared bitplane for debugging
    }
    g_refresh_needed = false;
  }

  // Display each bitplane with precise RMT-controlled timing
  // This implements Bit Code Modulation (BCM) for brightness control
  GPIO.out_w1ts.val = (1 << g_config.oe_pin); // LEDs off to avoid flickering!)
  display_bitplane(g_plane_idx);
  g_plane_idx = (g_plane_idx + 1) % BIT_DEPTH; // Increment plane index
}

/**
 * @brief Prepare bitplane data from framebuffer using Bit Code Modulation (BCM)
 * BCM allows multiple brightness levels by varying the time each LED is lit
 * @param plane The bit plane to prepare (0 = LSB, 1 = MSB for 2-bit depth)
 */
static void prepare_bitplane(uint8_t plane) {
  if (plane >= BIT_DEPTH) {
    ESP_LOGE(TAG, "Invalid bit plane %d requested", plane);
    return;
  }

  // Clear the bitplane buffer
  memset(g_bitplanes[plane], 0, BITPLANE_SIZE_BYTES);

  // Process each pixel in the framebuffer
  for (int y = 0; y < PANEL_HEIGHT; y++) {
    for (int x = 0; x < PANEL_WIDTH; x++) {
      uint8_t pixel_brightness = g_framebuffer[y][x]; // 0 to 2^BIT_DEPTH-1

      // Check if this bit plane should be lit for this pixel in the plane
      // by thresholding against the current bit value
      if (pixel_brightness >= (1 << plane)) {
        // Map logical pixel position to physical LED using the wiring LUT
        uint8_t physical_led = lut[y][x];
        uint8_t byte_idx = physical_led / 8;
        uint8_t bit_idx = 7 - (physical_led % 8); // MSB first bit ordering
        // set the corresponding bit in the bitplane
        g_bitplanes[plane][byte_idx] |= (1 << bit_idx);
      }
    }
  }
}

/**
 * @brief Send precisely timed OE pulse using RMT
 * Generates active-low pulse for the specified duration to enable LED output
 * @param duration_us Duration in microseconds to keep LEDs enabled
 */
static void rmt_send_oe_pulse(uint32_t duration_us) {
  // Create RMT symbol for OE pulse (active low timing)
  rmt_symbol_word_t oe_symbol = {
      .level0 = 0,              // OE active (low) - LEDs enabled
      .duration0 = duration_us, // Duration for this bitplane (BCM timing)
      .level1 = 1,              // OE inactive (high) - LEDs disabled
      .duration1 = 1,           // Brief high period before next operation
  };

  rmt_transmit_config_t tx_config = {
      .loop_count = 0,      // do not repeat signal
      .flags.eot_level = 1, // End of transmission level (1 = high)
      .flags.queue_nonblocking = 1,
  };
  rmt_transmit(g_rmt_oe, g_rmt_encoder, &oe_symbol, sizeof(oe_symbol),
               &tx_config);

  // Wait for transmission to complete before continuing
  // rmt_tx_wait_all_done(g_rmt_oe, portMAX_DELAY);
}

/**
 * @brief Display a single bitplane with precise timing
 * This implements one phase of BCM: SPI data transfer -> latch -> timed OE
 * pulse
 * @param plane The bitplane to display (0-1 for 2-bit BCM)
 */
static void display_bitplane(uint8_t plane) {
  // Step 1: Transfer bitplane data to shift registers via high-speed SPI
  spi_transaction_t trans = {
      .flags = 0,
      .length = (PANEL_WIDTH * PANEL_HEIGHT), // Length in bits
      .tx_buffer = g_bitplanes[plane],        // Prepared bitplane data
  };

  // Step 1: active blocking SPI transfer (not in ISR)
  esp_err_t ret = spi_device_polling_transmit(g_spi, &trans);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI transmission failed for plane %d: %s", plane,
             esp_err_to_name(ret));
    return;
  }

  // Step 2: Latch the data from shift registers to output registers
  latch_pulse();

  // Step 3: Enable LED output for precise duration using RMT
  // Shorter duration for LSB plane (fine brightness), longer for MSB plane
  // (coarse brightness)
  rmt_send_oe_pulse((plane_times_us[plane] * g_brightness_panel) / 255);
}

/////////////////////////
// DEBUG functions //
/////////////////////////

static void print_bitplane(uint8_t plane) {
  // print the newly prepared bitplane for debugging
  ESP_LOGI(TAG, "Prepared bitplane %d: ", plane);

  // 256 bits → 256 chars, +1 for NUL
  char line[256 + 1];

  // Fill in each bit
  for (size_t i = 0; i < BITPLANE_SIZE_BYTES; ++i) {
    for (int b = 0; b < 8; ++b) {
      // bit 7 of byte i goes to position i*8 + 0, etc.
      line[i * 8 + b] = (g_bitplanes[plane][i] & (1u << (7 - b))) ? '1' : '0';
    }
  }
  line[BITPLANE_SIZE_BYTES * 8] = '\0';

  // Single log call prints entire 256-bit string on one line
  ESP_LOGI(TAG, "%s", line);
}

static void print_framebuffer(void) {
  ESP_LOGI(TAG, "Current framebuffer state:");

  for (int i = 0; i < PANEL_HEIGHT; i++) {
    // Build one formatted line per row
    // the 3 is for 3 digits (max 255) + space and a final null terminator
    char line[4 * PANEL_WIDTH + 1] = {0};
    int pos = 0;
    for (int j = 0; j < PANEL_WIDTH; j++) {
      pos +=
          snprintf(line + pos, sizeof(line) - pos, "%3d ", g_framebuffer[i][j]);
    }
    // Ensure null-termination
    line[sizeof(line) - 1] = '\0'; // Safety null-termination
    // Log the formatted line
    ESP_LOGI(TAG, "%s", line);
  }
}

/////////////////////////////
// Initialization helper functions //
/////////////////////////////

static esp_err_t configure_gpio_drive_strength(void) {
  // Increase drive strength for SPI pins
  ESP_RETURN_ON_ERROR(
      gpio_set_drive_capability(g_config.clk_pin, GPIO_DRIVE_CAP_3), TAG,
      "Failed to set CLK drive strength");
  ESP_RETURN_ON_ERROR(
      gpio_set_drive_capability(g_config.di_pin, GPIO_DRIVE_CAP_3), TAG,
      "Failed to set DI drive strength");
  ESP_RETURN_ON_ERROR(
      gpio_set_drive_capability(g_config.latch_pin, GPIO_DRIVE_CAP_3), TAG,
      "Failed to set LATCH drive strength");
  ESP_RETURN_ON_ERROR(
      gpio_set_drive_capability(g_config.oe_pin, GPIO_DRIVE_CAP_3), TAG,
      "Failed to set OE drive strength");

  ESP_LOGI(TAG, "GPIO drive strength increased to maximum");
  return ESP_OK;
}

/**
 * @brief Setup RMT channel for precise OE (Output Enable) timing control
 * RMT provides microsecond-precision timing needed for BCM brightness control
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t rmt_setup_oe_channel(void) {
  // Configure RMT transmitter for OE pin control
  rmt_tx_channel_config_t tx_config = {
      .gpio_num = g_config.oe_pin,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 1000000, // 1MHz = 1µs resolution for precise timing
      .mem_block_symbols = 64,
      .trans_queue_depth = 1, // Single transaction at a time
      .flags.with_dma = false,
      .flags.invert_out = false,
  };
  ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_config, &g_rmt_oe), TAG,
                      "Failed to create RMT TX channel");

  // Create copy encoder for symbol transmission
  rmt_copy_encoder_config_t encoder_config = {};
  ESP_RETURN_ON_ERROR(rmt_new_copy_encoder(&encoder_config, &g_rmt_encoder),
                      TAG, "Failed to create RMT encoder");

  ESP_RETURN_ON_ERROR(rmt_enable(g_rmt_oe), TAG,
                      "Failed to enable RMT channel");
  return ESP_OK;
}

/**
 * @brief Initialize GPIO pins for latch and OE control
 * Configures pins as outputs and sets initial states using direct register
 * access
 * @return ESP_OK on success
 */
static esp_err_t init_gpio_pins(void) {
  ESP_LOGI(TAG, "Initializing GPIO pins (latch: %d, OE: %d)",
           g_config.latch_pin, g_config.oe_pin);

  // Configure GPIO pins for direct register access (faster than gpio_config)
  // Set pins as outputs
  GPIO.enable_w1ts.val = (1 << g_config.latch_pin) | (1 << g_config.oe_pin);

  GPIO.out_w1tc.val = (1 << g_config.latch_pin); // Latch low (idle state)
  GPIO.out_w1ts.val = (1 << g_config.oe_pin);    // Set high (LEDs off)

  configure_gpio_drive_strength(); // Increase drive strength for all relevant
                                   // pins

  return ESP_OK;
}

/**
 * @brief Initialize SPI interface for high-speed data transfer to shift
 * registers Configures SPI bus and device for optimal performance with LED
 * matrix
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t init_spi_interface(void) {
  ESP_LOGI(TAG, "Initializing SPI interface (host: %d, speed: %d Hz)",
           g_config.spi_host, g_config.spi_clock_speed_hz);

  // SPI bus configuration for shift register communication
  spi_bus_config_t bus_config = {
      .mosi_io_num = g_config.di_pin,         // Data input to shift registers
      .sclk_io_num = g_config.clk_pin,        // Clock for shift registers
      .miso_io_num = -1,                      // No MISO needed (output only)
      .quadwp_io_num = -1,                    // No quad mode
      .quadhd_io_num = -1,                    // No quad mode
      .max_transfer_sz = BITPLANE_SIZE_BYTES, // Maximum 32 bytes per transfer
      .flags = SPICOMMON_BUSFLAG_MASTER,
  };

  // SPI device configuration for optimal LED matrix performance
  spi_device_interface_config_t dev_config = {
      .clock_speed_hz = g_config.spi_clock_speed_hz,
      .mode = 0,                    // SPI mode 0 (CPOL=0, CPHA=0)
      .spics_io_num = -1,           // No CS pin (manual latch control)
      .queue_size = 1,              // Single transaction queue
      .flags = SPI_DEVICE_NO_DUMMY, // No dummy cycles needed
      // Add timing margins for shift register setup/hold times
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .input_delay_ns = 0,
  };

  // Initialize SPI bus and add device
  ESP_RETURN_ON_ERROR(
      spi_bus_initialize(g_config.spi_host, &bus_config, SPI_DMA_CH_AUTO), TAG,
      "SPI bus initialization failed");
  ESP_RETURN_ON_ERROR(
      spi_bus_add_device(g_config.spi_host, &dev_config, &g_spi), TAG,
      "SPI device add failed");

  return ESP_OK;
}

/**
 * @brief Initialize ESP Timer for precise 500Hz refresh timing
 * Creates and configures timer that will call refresh callback directly
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t init_refresh_timer(void) {
  ESP_LOGI(TAG, "Creating ESP timer for 500Hz refresh (%d µs period)",
           FRAME_PERIOD_US);

  // ESP Timer configuration for task callback execution
  esp_timer_create_args_t timer_args = {
      .callback = refresh_timer_callback,
      .name = "panel_refresh",
      .dispatch_method = ESP_TIMER_TASK, // Run in timer task context (not ISR,
                                         // safe for blocking ops)
  };

  ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &g_refresh_timer), TAG,
                      "ESP Timer creation failed");
  return ESP_OK;
}
