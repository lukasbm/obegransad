#include "led.h"

#include "driver/spi_master.h"
#include "driver/rmt.h"
#include "esp_timer.h"

uint8_t gBright = 100; // global brightness (0-255)

constexpr static uint8_t GRAY_LEVELS = 3; // number of grayscale values (besides black/off)

static spi_device_handle_t spi;
static rmt_item32_t plane_item[GRAY_LEVELS];
static const uint8_t *bitplane_data[GRAY_LEVELS]; // Zeiger auf 3 vorberechnete 32-Byte-Puffer
static volatile uint8_t current_plane = 0;

static void rmt_init()
{
    rmt_config_t cfg = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_CHANNEL_0,
        .gpio_num = (gpio_num_t)P_OE,
        .clk_div = 80, // 1 MHz Auflösung => 1 µs per Tick
        .mem_block_num = 1,
        .tx_config{
            .loop_en = false,
            .carrier_en = false,
            .idle_output_en = true,
            .idle_level = RMT_IDLE_LEVEL_HIGH // LEDs OFF when idle
        }};
    rmt_config(&cfg);
    rmt_driver_install(cfg.channel, 0, 0);

    auto makeItem = [](uint32_t on_us) -> rmt_item32_t
    {
        return (rmt_item32_t){
            .duration0 = on_us, .level0 = 1, // OE aktiv
            .duration1 = FRAME_TIME_US - on_us,
            .level1 = 0};
    };
    plane_item[0] = makeItem(PLANE0_ON_US);
    plane_item[1] = makeItem(PLANE1_ON_US);
    plane_item[2] = makeItem(PLANE2_ON_US);
}

static void spi_init()
{
    spi_bus_config_t bus = {
        .mosi_io_num = P_DI,
        .miso_io_num = -1,
        .sclk_io_num = P_CLK,
        .max_transfer_sz = 32}; // 256 bits
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = SPI_HZ,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = GRAY_LEVELS};
    spi_bus_add_device(SPI2_HOST, &dev, &spi);
}

// high performance latch pulse
IRAM_ATTR inline void latch_pulse()
{
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << P_LATCH); // LAT = 1
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << P_LATCH); // LAT = 0
}

void IRAM_ATTR refresh_isr()
{
    const uint8_t plane = current_plane;

    /* 1) LEDs off → OE High (RMT is off) */
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << P_OE);

    /* 2) 256 Bit via SPI DMA (nonblocking) */
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = BIT_COUNT, // of one data plane
        .tx_buffer = bitplane_data[plane]};
    spi_device_queue_trans(spi, &t, 0); // don't wait actively

    /* 3) Latch when DMA SPI transfer complete (wait passively, <14 µs) */
    spi_transaction_t *r;
    spi_device_get_trans_result(spi, &r, portMAX_DELAY);
    latch_pulse();

    /* 4) OE-window for this plane via RMT */
    rmt_write_items(RMT_CHANNEL_0, &plane_item[plane], 1, false);

    current_plane = (plane + 1) % 3;
}

void panel_init()
{
    pinMode(P_LATCH, OUTPUT); // LA/ (active‑low latch); STCP LATCH

    spi_init();
    rmt_init();

    // FIXME: set pointer to the bitplane data
    bitplane_data[0] = panel_buf;
}

void panel_show()
{
    refresh_isr();
}

void panel_timer_start()
{
    /* High-res-Timer → 300 Hz */
    const esp_timer_create_args_t ta = {
        .callback = &refresh_isr,
        .name = "panel300"};
    esp_timer_handle_t h;
    esp_timer_create(&ta, &h);
    esp_timer_start_periodic(h, FRAME_TIME_US); // 3333 µs
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
                panel_setPixel(tlY + y, tlX + x, colorMap[pixel]);
            }
            i++;
        }
    }
}

void panel_print(void)
{
    Serial.println("Panel buffer:");
    for (int i = 0; i < 256; i++)
    {
        if (i % 16 == 0)
        {
            Serial.println();
        }
        Serial.print(panel_buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}
