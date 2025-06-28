#include "led.h"

#include <driver/spi_master.h>
#include <esp32-hal-rmt.h>
#include <soc/rmt_struct.h>
#include <soc/rmt_reg.h>

uint8_t gBright = 100; // global brightness (0-255)
DRAM_ATTR static uint8_t plane0[32];
DRAM_ATTR static uint8_t plane1[32];
DRAM_ATTR static uint8_t plane2[32];
static const uint8_t *planes[3] = {plane0, plane1, plane2};
static volatile uint8_t planeIdx = 0;
static spi_device_handle_t spi;
static spi_transaction_t trans[3]; // 3 prepared transactions
static hw_timer_t *panelTimer = nullptr;
static rmt_obj_t *rmtOE;

// Quick RMT starter
static IRAM_ATTR inline void rmt_oe_start(uint32_t on_us)
{
    RMTMEM.chan[0].data32[0].duration0 = on_us;
    RMTMEM.chan[0].data32[0].level0 = 0; // OE aktiv LOW
    RMTMEM.chan[0].data32[0].duration1 = FRAME_US - on_us;
    RMTMEM.chan[0].data32[0].level1 = 1;
    RMT.channel[0].conf1.tx_start = 1;
}

// high performance latch pulse
static IRAM_ATTR inline void latch_pulse()
{
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << P_LATCH); // LAT = 1
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << P_LATCH); // LAT = 0
}

static inline void IRAM_ATTR oePulse(uint32_t on)
{
    rmt_item32_t it{on, 0, FRAME_US - on, 1}; // OE aktiv LOW
    rmtWrite(rmtOE, &it, 1, false);
}

// SPI transfer complete callback --> Latch + OE Window
static void IRAM_ATTR spi_done_cb(spi_transaction_t *t)
{
    if (planeIdx == 0)
        oePulse(PLANE0_ON_US);
    else if (planeIdx == 1)
        oePulse(PLANE1_ON_US);
    else
        oePulse(PLANE2_ON_US);
    latch_pulse();
}

// HW-Timer ISR callback
void IRAM_ATTR panel_isr()
{
    /* NUR ISR-safe-Funktion!  queue_trans _ISR sperrt keine Mutexe */
    static spi_transaction_t trans{};
    trans.length = 256;
    trans.tx_buffer = planes[planeIdx];
    trans.user = nullptr;
    spi_device_queue_trans_ISR(spi, &trans, nullptr);

    planeIdx = (planeIdx + 1) % 3;
}

static void init_spi()
{
    spi_bus_config_t bus{.mosi_io_num = P_DI,
                         .miso_io_num = -1,
                         .sclk_io_num = P_CLK,
                         .max_transfer_sz = 32};
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev{
        .clock_speed_hz = SPI_HZ,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 2,
        .post_cb = spi_done_cb};
    spi_bus_add_device(SPI2_HOST, &dev, &spi);
}

static void init_rmt()
{
    rmtOE = rmtInit(P_OE, false, RMT_CHANNEL_0, 80); // 1-µs-Ticks
}

static void init_timer()
{
    panelTimer = timerBegin(0, 80, true); // 1us tick
    timerAttachInterrupt(panelTimer, &panel_isr, true);
    timerAlarmWrite(panelTimer, FRAME_TIME_US, true);
    timerAlarmEnable(panelTimer);
}

void panel_init()
{
    pinMode(P_LATCH, OUTPUT); // LA/ (active‑low latch); STCP LATCH
    init_rmt();
    init_spi();
    init_timer();
}

void panel_show()
{
    // refresh_isr();
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
