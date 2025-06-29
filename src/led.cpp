#include "led.h"

#include <driver/spi_master.h>
#include <driver/rmt.h>

uint8_t gBright = 100; // global brightness (0-255)

// handles
static volatile uint8_t planeIdx = 0;
static spi_device_handle_t spi;
static hw_timer_t *panelTimer = nullptr;

// image buffers
DRAM_ATTR static uint8_t plane0[32];
DRAM_ATTR static uint8_t plane1[32];
DRAM_ATTR static uint8_t plane2[32];
constexpr const uint8_t *planes[3] = {plane0, plane1, plane2};

// RMT items (OE active low, 1 µs pulse)
DRAM_ATTR static rmt_item32_t rmt_plane0 = {PLANE0_ON_US, 0, FRAME_US - PLANE0_ON_US, 1};
DRAM_ATTR static rmt_item32_t rmt_plane1 = {PLANE1_ON_US, 0, FRAME_US - PLANE1_ON_US, 1};
DRAM_ATTR static rmt_item32_t rmt_plane2 = {PLANE2_ON_US, 0, FRAME_US - PLANE2_ON_US, 1};
constexpr rmt_item32_t *rmt_planes[3] = {&rmt_plane0, &rmt_plane1, &rmt_plane2};

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

/* ----------- Timer-ISR (300 Hz) ----------------------------- */
void IRAM_ATTR panel_isr()
{
    static spi_transaction_t trans{};
    trans.length = 256; // 256 Bit
    trans.tx_buffer = planes[planeIdx];
    trans.user = reinterpret_cast<void *>(planeIdx);

    /* ISR-tauglich: 0 Ticks timeout  */
    if (spi_device_queue_trans(spi, &trans, 0) == ESP_OK)
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
        .max_transfer_sz = 32,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_HZ,
        .spics_io_num = -1,
        .queue_size = 3,
        .post_cb = spi_done_cb};

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
    timerAlarmEnable(panelTimer);
}

void panel_init()
{
    pinMode(P_LATCH, OUTPUT); // LA/ (active‑low latch); STCP LATCH
    init_spi();
    init_rmt();
    init_timer();
}

// TODO: ab hier

void panel_refresh()
{
    /* Demo-Bildinhalt generieren */
    static uint8_t c = 0;
    memset(plane0, c, sizeof plane0);
    memset(plane1, c ^ 85, sizeof plane1);
    memset(plane2, c ^ 170, sizeof plane2);
    c++;
    // refresh_isr();
}

void panel_timer_start()
{
}

void panel_timer_stop()
{
}

void panel_drawSprite(int8_t tlX, int8_t tlY, const uint8_t *data, uint8_t width, uint8_t height)
{
    uint8_t *p = const_cast<uint8_t *>(data);
    uint8_t i = 0;
    // move over entire sprite (2 bits per pixel)
    for (uint8_t y = 0; y < height; y++)
    {
        for (uint8_t x = 0; x < width; x++)
        {
            // move to next byte
            if (i >= 4)
            {
                p++;
                i = 0;
            }
            // get the pixel value (2 bits per pixel) - they are big endian.
            // draw it (if not out of bounds)
            if ((tlX + x >= 0) && (tlX + x < 16) && (tlY + y >= 0) && (tlY + y < 16))
            {
                // set pixel in the panel buffer
                uint8_t pixel = (*p >> (6 - i * 2)) & mask;
                // panel_setPixel(tlY + y, tlX + x, colorMap[pixel]);
            }
            i++;
        }
    }
}

void panel_hold()
{
    digitalWrite(P_OE, LOW); // OE/ LOW  → LEDs ON
}

void panel_fill(uint8_t col)
{
    // TODO: implement!
}

void panel_setPixel(int8_t row, int8_t col, uint8_t brightness)
{
}