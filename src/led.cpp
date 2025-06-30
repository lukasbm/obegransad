#include "led.h"

#include <driver/spi_master.h>
#include <driver/rmt.h>

uint8_t gBright = 100; // global brightness (0-255)

// handles
static volatile uint8_t planeIdx = 0;
static spi_device_handle_t spi;
static hw_timer_t *panelTimer = nullptr;

// image buffers
DRAM_ATTR static Brightness framebuffer[ROWS][COLS];
DRAM_ATTR static uint8_t plane0[32];
DRAM_ATTR static uint8_t plane1[32];
DRAM_ATTR static uint8_t plane2[32];
constexpr const uint8_t *planes[3] = {plane0, plane1, plane2};

// RMT items (OE active low, 1 µs pulse)
DRAM_ATTR static rmt_item32_t rmt_plane0 = {PLANE0_ON_US, 0, FRAME_US - PLANE0_ON_US, 1};
DRAM_ATTR static rmt_item32_t rmt_plane1 = {PLANE1_ON_US, 0, FRAME_US - PLANE1_ON_US, 1};
DRAM_ATTR static rmt_item32_t rmt_plane2 = {PLANE2_ON_US, 0, FRAME_US - PLANE2_ON_US, 1};
constexpr rmt_item32_t *rmt_planes[3] = {&rmt_plane0, &rmt_plane1, &rmt_plane2};

// One transaction descriptor for each plane to avoid race conditions in the ISR
DRAM_ATTR static spi_transaction_t trans[3];

// high performance latch pulse
static IRAM_ATTR inline void latch_pulse()
{
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << P_LATCH); // LAT = 1
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << P_LATCH); // LAT = 0
}

/* ----------- SPI-Callback: Latch + OE ----------------------- */
static void IRAM_ATTR spi_done_cb(spi_transaction_t *t)
{
    latch_pulse();
    /* OE-window (1 Item) – ISR-safe */
    rmt_write_items(RMT_CHANNEL_0, rmt_planes[(uint32_t)t->user], 1, false);
}

void IRAM_ATTR panel_isr()
{
    /* ISR compatible: 0 Ticks timeout  */
    // Queue the transaction for the current plane. The transaction descriptor is already configured in init_spi().
    if (spi_device_queue_trans(spi, &trans[planeIdx], 0) == ESP_OK)
    {
        planeIdx = (planeIdx + 1) % 3;
    }
}

static void init_spi()
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = P_DI,
        .miso_io_num = -1,
        .sclk_io_num = P_CLK,
        .max_transfer_sz = BIT_COUNT / 8,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    // as per SCT2024 datasheet, data is sampled at the rising edge of CLK (idle low).
    spi_device_interface_config_t devcfg = {
        .mode = 0, // CPOL=0, CPHA=0. Clock is idle low, data is sampled on rising edge.
        .clock_speed_hz = SPI_HZ,
        .spics_io_num = -1,
        .queue_size = 3,
        .post_cb = spi_done_cb};

    // Initialize the transaction descriptors.
    // It's CRITICAL to zero them out first, otherwise uninitialized flags can cause issues.
    for (int i = 0; i < 3; i++)
    {
        memset(&trans[i], 0, sizeof(spi_transaction_t)); // Zero out the transaction
        trans[i].length = BIT_COUNT;
        trans[i].tx_buffer = planes[i];
        trans[i].user = reinterpret_cast<void *>(i);
    }

    spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
}

static void init_rmt()
{
    rmt_tx_config_t tx_config = {
        .idle_level = RMT_IDLE_LEVEL_HIGH, // LEDs standard OFF
        .carrier_en = false,               // Carrier enable
        .loop_en = false,                  // Loop enable
        .idle_output_en = true,
    };

    rmt_config_t cfg = {
        .rmt_mode = RMT_MODE_TX,      // RMT mode: transmit
        .channel = RMT_CHANNEL_0,     // RMT channel
        .gpio_num = (gpio_num_t)P_OE, // GPIO pin for RMT output
        .clk_div = 80,                // Clock divider (80MHz / 80 = 1MHz -> 1 µs tick)
        .mem_block_num = 1,           // Number of memory blocks
        .tx_config = tx_config};
    rmt_config(&cfg);
    rmt_driver_install(cfg.channel, 0, 0); // keine RX-Puffer
}

static void init_timer()
{
    panelTimer = timerBegin(0, 80, true); // 1 µs Auflösung
    timerAttachInterrupt(panelTimer, &panel_isr, true);
    timerAlarmWrite(panelTimer, FRAME_TIME_US, true);
}

void panel_init()
{
    pinMode(P_LATCH, OUTPUT); // LA/ (active‑low latch); STCP LATCH
    init_spi();
    init_rmt();
    init_timer();
    panel_timer_start(); // Start the refresh cycle
}

void panel_timer_start()
{
    if (panelTimer != nullptr)
    {
        timerAlarmEnable(panelTimer);
    }
}

void panel_timer_stop()
{
    if (panelTimer != nullptr)
    {
        timerAlarmDisable(panelTimer);
    }
}

void panel_fill(Brightness col)
{
    for (uint8_t row = 0; row < ROWS; row++)
    {
        for (uint8_t colIdx = 0; colIdx < COLS; colIdx++)
        {
            framebuffer[row][colIdx] = col;
        }
    }
}

void panel_setPixel(uint8_t row, uint8_t col, Brightness brightness)
{
    if (row < ROWS && col < COLS)
    {
        framebuffer[row][col] = brightness;
    }
}

void panel_commit()
{
    // This function translates the logical framebuffer into the physical bit-plane buffers
    // that the ISR uses for display refresh.

    // First, clear all plane buffers. This is faster than conditionally clearing bits.
    memset(plane0, 0, sizeof(plane0));
    memset(plane1, 0, sizeof(plane1));
    memset(plane2, 0, sizeof(plane2));

    for (uint8_t r = 0; r < ROWS; r++)
    {
        for (uint8_t c = 0; c < COLS; c++)
        {
            uint8_t b_val = static_cast<uint8_t>(framebuffer[r][c]);
            if (b_val == 0)
                continue; // Skip if pixel is off

            int pixel_idx = lut[r][c];
            int byte_idx = pixel_idx / 8;
            uint8_t bit_mask = 1 << (pixel_idx % 8);

            // Correctly implement Binary Coded Modulation (BCM)
            // A brightness of 3 (0b11) should light up plane0 AND plane1.
            // The switch statement was mutually exclusive and incorrect.
            if (b_val & 0b01) // For BRIGHTNESS_1 and BRIGHTNESS_3
            {
                plane0[byte_idx] |= bit_mask;
            }
            if (b_val & 0b10) // For BRIGHTNESS_2 and BRIGHTNESS_3
            {
                plane1[byte_idx] |= bit_mask;
            }
        }
    }
}
