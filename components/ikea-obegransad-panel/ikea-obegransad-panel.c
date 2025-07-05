#include "ikea-obegransad-panel.h"

// #include "freertos/task.h"
// #include "driver/spi_master.h"
// #include "driver/rmt_tx.h"
// #include "driver/gptimer.h"
// #include "esp_log.h"
// #include "esp_check.h"

static const char *TAG = "obegransad_panel";

#define BITPLANE_SIZE_BYTES ((PANEL_WIDTH * PANEL_HEIGHT) / 8)
#define FRAMEBUFFER_SIZE_BYTES (PANEL_WIDTH * PANEL_HEIGHT)

// LUT For OBEGRÄNSAD panel wiring
// static const uint8_t lut[16][16] = {
//     {23, 22, 21, 20, 19, 18, 17, 16, 7, 6, 5, 4, 3, 2, 1, 0},
//     {24, 25, 26, 27, 28, 29, 30, 31, 8, 9, 10, 11, 12, 13, 14, 15},
//     {39, 38, 37, 36, 35, 34, 33, 32, 55, 54, 53, 52, 51, 50, 49, 48},
//     {40, 41, 42, 43, 44, 45, 46, 47, 56, 57, 58, 59, 60, 61, 62, 63},
//     {87, 86, 85, 84, 83, 82, 81, 80, 71, 70, 69, 68, 67, 66, 65, 64},
//     {88, 89, 90, 91, 92, 93, 94, 95, 72, 73, 74, 75, 76, 77, 78, 79},
//     {103, 102, 101, 100, 99, 98, 97, 96, 119, 118, 117, 116, 115, 114, 113, 112},
//     {104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127},
//     {151, 150, 149, 148, 147, 146, 145, 144, 135, 134, 133, 132, 131, 130, 129, 128},
//     {152, 153, 154, 155, 156, 157, 158, 159, 136, 137, 138, 139, 140, 141, 142, 143},
//     {167, 166, 165, 164, 163, 162, 161, 160, 183, 182, 181, 180, 179, 178, 177, 176},
//     {168, 169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190, 191},
//     {215, 214, 213, 212, 211, 210, 209, 208, 199, 198, 197, 196, 195, 194, 193, 192},
//     {216, 217, 218, 219, 220, 221, 222, 223, 200, 201, 202, 203, 204, 205, 206, 207},
//     {231, 230, 229, 228, 227, 226, 225, 224, 247, 246, 245, 244, 243, 242, 241, 240},
//     {232, 233, 234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254, 255}
// };

// static panel_config_t g_panel_config;
// static spi_device_handle_t g_spi_handle;
// static rmt_channel_handle_t g_rmt_channel = NULL;
// static rmt_encoder_handle_t g_rmt_oe_encoders[BIT_DEPTH] = {NULL};
// static gptimer_handle_t g_gptimer = NULL;
// static TaskHandle_t g_refresh_task_handle = NULL;

// Double buffering for bitplanes
// static uint8_t* g_bitplanes[2];
// static volatile int g_active_bitplanes_idx = 0;

// // User-facing framebuffer
// static uint8_t* g_framebuffer;

// static void prepare_bitplanes(void);
// static void refresh_task(void *arg);
// static bool panel_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);

// esp_err_t panel_init(const panel_config_t *config) {
//     ESP_LOGI(TAG, "Initializing panel driver");
//     g_panel_config = *config;

//     // --- GPIO Init ---
//     gpio_config_t io_conf = {
//         .pin_bit_mask = (1ULL << g_panel_config.latch_pin),
//         .mode = GPIO_MODE_OUTPUT,
//         .intr_type = GPIO_INTR_DISABLE,
//     };
//     ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "GPIO config failed");
//     gpio_set_level(g_panel_config.latch_pin, 0);

//     // --- SPI Init ---
//     spi_bus_config_t buscfg = {
//         .mosi_io_num = g_panel_config.di_pin,
//         .sclk_io_num = g_panel_config.clk_pin,
//         .miso_io_num = -1,
//         .quadwp_io_num = -1,
//         .quadhd_io_num = -1,
//         .max_transfer_sz = BITPLANE_SIZE_BYTES,
//     };
//     spi_device_interface_config_t devcfg = {
//         .clock_speed_hz = g_panel_config.spi_clock_speed_hz,
//         .mode = 0,
//         .spics_io_num = -1,
//         .queue_size = 1,
//     };
//     ESP_RETURN_ON_ERROR(spi_bus_initialize(g_panel_config.spi_host, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI bus init failed");
//     ESP_RETURN_ON_ERROR(spi_bus_add_device(g_panel_config.spi_host, &devcfg, &g_spi_handle), TAG, "SPI device add failed");

//     // --- RMT Init ---
//     rmt_tx_channel_config_t rmt_chan_config = {
//         .gpio_num = g_panel_config.oe_pin,
//         .clk_src = RMT_CLK_SRC_DEFAULT,
//         .resolution_hz = 1000000, // 1MHz resolution, 1 tick = 1us
//         .mem_block_symbols = 64,
//         .trans_queue_depth = 4,
//         .flags.invert_out = true, // OE is active low
//     };
//     ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&rmt_chan_config, &g_rmt_channel), TAG, "RMT channel creation failed");

//     // Create RMT encoders for each bitplane's OE pulse
//     const uint32_t oe_on_us[] = {100, 250}; // Brightness weights for BCM
//     for (int i = 0; i < BIT_DEPTH; i++) {
//         rmt_bytes_encoder_config_t bytes_encoder_config = {
//             .bit0 = { .level0 = 1, .duration0 = oe_on_us[i], .level1 = 0, .duration1 = 0 },
//             .bit1 = { .level0 = 1, .duration0 = oe_on_us[i], .level1 = 0, .duration1 = 0 },
//             .flags.msb_first = true
//         };
//         ESP_RETURN_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &g_rmt_oe_encoders[i]), TAG, "RMT encoder creation failed");
//     }
//     ESP_RETURN_ON_ERROR(rmt_enable(g_rmt_channel), TAG, "RMT enable failed");

//     // --- GPtimer Init ---
//     gptimer_config_t timer_config = {
//         .clk_src = GPTIMER_CLK_SRC_DEFAULT,
//         .direction = GPTIMER_COUNT_UP,
//         .resolution_hz = 1000000, // 1MHz
//     };
//     ESP_RETURN_ON_ERROR(gptimer_new_timer(&timer_config, &g_gptimer), TAG, "GPTimer creation failed");

//     gptimer_alarm_config_t alarm_config = {
//         .alarm_count = 2500, // 400Hz refresh rate (1s / 400 = 2500us)
//         .reload_count = 0,
//         .flags.auto_reload_on_alarm = true,
//     };
//     gptimer_event_callbacks_t cbs = { .on_alarm = panel_timer_callback };
//     ESP_RETURN_ON_ERROR(gptimer_register_event_callbacks(g_gptimer, &cbs, NULL), TAG, "GPTimer callback registration failed");
//     ESP_RETURN_ON_ERROR(gptimer_set_alarm_action(g_gptimer, &alarm_config), TAG, "GPTimer alarm action failed");
//     ESP_RETURN_ON_ERROR(gptimer_enable(g_gptimer), TAG, "GPTimer enable failed");

//     // --- Buffer Allocation ---
//     g_framebuffer = heap_caps_malloc(FRAMEBUFFER_SIZE_BYTES, MALLOC_CAP_8BIT);
//     g_bitplanes[0] = heap_caps_malloc(BITPLANE_SIZE_BYTES * BIT_DEPTH, MALLOC_CAP_DMA);
//     g_bitplanes[1] = heap_caps_malloc(BITPLANE_SIZE_BYTES * BIT_DEPTH, MALLOC_CAP_DMA);
//     ESP_RETURN_ON_FALSE(g_framebuffer && g_bitplanes[0] && g_bitplanes[1], ESP_ERR_NO_MEM, TAG, "Failed to allocate buffers");

//     memset(g_framebuffer, 0, FRAMEBUFFER_SIZE_BYTES);
//     memset(g_bitplanes[0], 0, BITPLANE_SIZE_BYTES * BIT_DEPTH);
//     memset(g_bitplanes[1], 0, BITPLANE_SIZE_BYTES * BIT_DEPTH);

//     // --- Task Creation ---
//     xTaskCreatePinnedToCore(refresh_task, "panel_refresh", 4096, NULL, 5, &g_refresh_task_handle, 1);

//     // --- Start Machine ---
//     ESP_RETURN_ON_ERROR(gptimer_start(g_gptimer), TAG, "GPTimer start failed");
//     ESP_LOGI(TAG, "Panel driver initialized successfully");
//     return ESP_OK;
// }

// uint8_t* panel_get_framebuffer(void) {
//     return g_framebuffer;
// }

// void panel_commit(void) {
//     // Prepare the next frame in the inactive buffer
//     prepare_bitplanes();
//     // Atomically swap to the newly prepared buffer
//     g_active_bitplanes_idx = 1 - g_active_bitplanes_idx;
// }

// static void prepare_bitplanes(void) {
//     // Select the inactive buffer to write to
//     uint8_t* current_bitplanes = g_bitplanes[1 - g_active_bitplanes_idx];
//     memset(current_bitplanes, 0, BITPLANE_SIZE_BYTES * BIT_DEPTH);

//     for (int y = 0; y < PANEL_HEIGHT; y++) {
//         for (int x = 0; x < PANEL_WIDTH; x++) {
//             uint8_t brightness = g_framebuffer[y * PANEL_WIDTH + x] & 0x03; // Mask to 2 bits
//             if (brightness == 0) continue;

//             int physical_idx = lut[y][x];
//             int byte_idx = physical_idx / 8;
//             int bit_idx = 7 - (physical_idx % 8); // MSB first

//             if (brightness & 0x01) { // Bit 0 of brightness -> Plane 0
//                 current_bitplanes[0 * BITPLANE_SIZE_BYTES + byte_idx] |= (1 << bit_idx);
//             }
//             if (brightness & 0x02) { // Bit 1 of brightness -> Plane 1
//                 current_bitplanes[1 * BITPLANE_SIZE_BYTES + byte_idx] |= (1 << bit_idx);
//             }
//         }
//     }
// }

// static bool IRAM_ATTR panel_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
//     BaseType_t high_task_woken = pdFALSE;
//     vTaskNotifyGiveFromISR(g_refresh_task_handle, &high_task_woken);
//     return high_task_woken == pdTRUE;
// }

// static void refresh_task(void *arg) {
//     spi_transaction_t spi_trans = {
//         .length = BITPLANE_SIZE_BYTES * 8,
//     };
//     rmt_transmit_config_t tx_config = { .loop_count = 0 };
//     const uint8_t rmt_oe_payload = 0xFF; // Dummy payload to trigger the encoder

//     while (1) {
//         ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

//         uint8_t* current_bitplanes = g_bitplanes[g_active_bitplanes_idx];

//         for (int plane = 0; plane < BIT_DEPTH; plane++) {
//             // 1. SPI Transfer
//             spi_trans.tx_buffer = &current_bitplanes[plane * BITPLANE_SIZE_BYTES];
//             ESP_ERROR_CHECK(spi_device_polling_transmit(g_spi_handle, &spi_trans));

//             // 2. Latch data
//             gpio_set_level(g_panel_config.latch_pin, 1);
//             gpio_set_level(g_panel_config.latch_pin, 0);

//             // 3. OE Pulse with RMT
//             ESP_ERROR_CHECK(rmt_transmit(g_rmt_channel, g_rmt_oe_encoders[plane], &rmt_oe_payload, sizeof(rmt_oe_payload), &tx_config));
//             // Wait for the OE pulse to finish to ensure correct timing
//             ESP_ERROR_CHECK(rmt_tx_wait_all_done(g_rmt_channel, portMAX_DELAY));
//         }
//     }
// }