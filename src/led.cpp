#include "led.h"

#include <driver/spi_master.h>
#include <esp32-hal-rmt.h>

uint8_t gBright = 100; // global brightness (0-255)
DRAM_ATTR static uint8_t plane0[32];
DRAM_ATTR static uint8_t plane1[32];
DRAM_ATTR static uint8_t plane2[32];
static const uint8_t *planes[3] = {plane0, plane1, plane2};
static volatile uint8_t current_plane = 0;
static volatile uint8_t next_plane = 0;
static spi_device_handle_t spi;
static spi_transaction_t trans[3]; // 3 prepared transactions

// Quick RMT starter
static inline void rmt_oe_start(uint32_t on_us) IRAM_ATTR
{
    RMTMEM.chan[0].data32[0].duration0 = on_us;
    RMTMEM.chan[0].data32[0].level0 = 0; // OE aktiv LOW
    RMTMEM.chan[0].data32[0].duration1 = FRAME_US - on_us;
    RMTMEM.chan[0].data32[0].level1 = 1;
    RMT.channel[0].conf1.tx_start = 1;
}

// high performance latch pulse
static inline void latch_pulse() IRAM_ATTR
{
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << P_LATCH); // LAT = 1
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << P_LATCH); // LAT = 0
}

static inline void IRAM_ATTR oePulse(uint32_t on)
{
    rmt_item32_t it{on, 0, FRAME_US - on, 1}; // OE aktiv LOW
    rmtWriteItems(rmtOE, &it, 1, false);
}

// SPI transfer complete callback --> Latch + OE Window
static void IRAM_ATTR spi_done_cb(spi_transaction_t *t)
{
    uint32_t plane = (uint32_t)t->user;
    latch_pulse();
    if (plane == 0)
        rmt_oe_start(PL0_ON_US);
    else if (plane == 1)
        rmt_oe_start(PL1_ON_US);
    else
        rmt_oe_start(PL2_ON_US);
}

// HW-Timer ISR callback
static bool IRAM_ATTR gptimer_isr_cb(gptimer_handle_t, const gptimer_alarm_event_data_t *, void *)
{
    spi_device_queue_trans(spi, &trans[next_plane], 0); // remove immediately
    next_plane = (next_plane + 1) % 3;
    return false; // no yield
}
static void init_spi()
{
    spi_bus_config_t bus{.mosi_io_num = PIN_MOSI, .miso_io_num = -1, .sclk_io_num = PIN_CLK, .max_transfer_sz = 32};
    spi_bus_initialize(SPI_HOST, &bus, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev{
        .clock_speed_hz = 20'000'000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 3,
        .post_cb = spi_done_cb};
    spi_bus_add_device(SPI_HOST, &dev, &spi);

    /* 3 Transaktionen vorbereiten */
    for (int i = 0; i < 3; ++i)
    {
        trans[i] = {};
        trans[i].length = 256;
        trans[i].tx_buffer = planes[i];
        trans[i].user = (void *)i;
    }
}

static void init_rmt()
{
    periph_module_enable(PERIPH_RMT_MODULE);
    RMT.channel[0].conf0.div_cnt = 80; // 1 µs Ticks
    RMT.channel[0].conf0.mem_size = 1;
    RMT.channel[0].conf0.carrier_en = 0;
    gpio_matrix_out(PIN_OE, RMT_SIG_OUT0_IDX, true, false);
}

static void init_timer()
{
    /* GPTimer 0 @ APB 80 MHz */
    gptimer_handle_t h;
    gptimer_config_t cfg{
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1'000'000 // 1 µs
    };
    gptimer_new_timer(&cfg, &h);

    gptimer_alarm_config_t alarm{.alarm_count = REFRESH_US, .reload_count = 0, .flags{.auto_reload_on_alarm = true}};
    gptimer_set_alarm_action(h, &alarm);

    gptimer_event_callbacks_t cbs{.on_alarm = gptimer_isr_cb};
    gptimer_register_event_callbacks(h, &cbs, nullptr);
    gptimer_enable(h);
    gptimer_start(h);
}

/// TODO: ab hier

void IRAM_ATTR fill_demo()
{
    static uint8_t frame = 0;
    memset(plane0, frame, sizeof plane0);
    memset(plane1, frame ^ 0x55, sizeof plane1);
    memset(plane2, frame ^ 0xAA, sizeof plane2);
    frame++;
}

void panel_init()
{
    pinMode(P_LATCH, OUTPUT); // LA/ (active‑low latch); STCP LATCH
    init_spi();
    init_rmt();
    init_timer();
}

void panel_show()
{
    // FIXME: fix
    // refresh_isr();
    fill_demo(); // fill the demo pattern
}

// TODO: ab hier

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
