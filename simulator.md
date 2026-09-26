Yes. Your proposed architecture is reasonable, but I would not start by trying to emulate the ESP32-C3 peripherals perfectly in QEMU. For this project, I’d split the firmware so that the display-driving layer can be replaced by a host-side “virtual display backend,” while keeping a smaller amount of real-hardware testing for SPI/RMT timing.

ESP32-C3 is supported by Espressif’s QEMU tooling, and current ESP-IDF can launch firmware under QEMU with `idf.py qemu`. Espressif’s emulator work also includes an ESP32-C3 emulator with Wi-Fi station emulation and host networking. ([Espressif Systems][1]) The problem is that “CPU boots and Wi-Fi works” is quite different from “my exact SPI + RMT + DMA behavior is faithfully emulated.” That latter part is where you’re likely to spend a lot of effort.

For your 16×16 monochrome bitplane display, I’d structure it roughly like this:

```text
                 your application
                       |
             display/frame logic
                       |
              bitplane encoder
                       |
          +------------+------------+
          |                         |
    ESP32 hardware             host simulator
      backend                    backend
   SPI + RMT + GPIO       socket/file/SDL/etc.
          |                         |
  shift registers             16x16 window
```

The important boundary is after the bitplane generation but before the actual `spi_device_transmit()` / RMT calls.

For example, instead of application code directly doing this:

```c
spi_device_transmit(spi, &trans);
rmt_transmit(...);
```

have something like:

```c
display_backend_send_bitplane(
    plane,
    row,
    duration_ticks
);
```

Then your ESP implementation turns that into SPI/RMT transactions:

```c
void display_backend_send_bitplane(...)
{
    spi_device_transmit(...);
    rmt_transmit(...);
}
```

while the host implementation does:

```c
void display_backend_send_bitplane(...)
{
    simulator_apply_shift_data(...);
    simulator_set_row(...);
    simulator_display_for(duration_ticks);
}
```

That gives you a much better development loop. Your entire framebuffer logic, animations, Wi-Fi protocol handling, ESP-NOW packet interpretation, bitplane generation, row scanning logic, etc. can be tested without the physical display.

ESP-IDF explicitly supports host-based testing/mocking as a development strategy, although Espressif notes that target-device testing is still necessary for integration and hardware-specific behavior. ([Espressif Systems][2])

There is also an interesting middle ground: Wokwi.

Wokwi currently supports the Seeed XIAO ESP32-C3 specifically, can run custom ESP-IDF firmware, supports SPI, supports transmit-side RMT to some extent, and provides simulated Wi-Fi with internet connectivity. ([Wokwi Docs][3])

So this could potentially get you surprisingly close:

```text
ESP-IDF firmware
      |
 virtual ESP32-C3
      |
 SPI pins / GPIO / RMT
      |
 custom simulated peripheral
      |
 simulated 16x16 display
```

Wokwi has a custom-chip API, so you can implement something resembling your shift-register chain and interpret the clock/data/latch/row signals yourself. That would be quite attractive because then you are testing the actual GPIO/SPI-level behavior rather than introducing a higher-level fake backend.

The main caveat is RMT. Wokwi describes its ESP32 RMT support as partial/transmit-oriented, primarily for things like WS2812 driving. ([Wokwi Docs][3]) Depending on how you're using RMT — e.g. as a precise OE/row timing generator — it might not reproduce the behavior you care about.

ESP-NOW is the awkward part.

Ordinary IP networking is comparatively easy to emulate because it can be translated to host networking. ESP-NOW is below IP and uses vendor-specific 802.11 action frames, so simulators generally can't just map it onto a normal host socket. I would not design your development workflow around simulated ESP-NOW.

Instead, put a small abstraction around that too:

```c
typedef struct {
    uint8_t src[6];
    uint8_t data[250];
    size_t len;
} display_packet_t;

void display_packet_received(const display_packet_t *p);
```

On actual hardware:

```text
ESP-NOW callback
       ↓
display_packet_received()
```

On the PC:

```text
UDP / Unix socket / test program
       ↓
display_packet_received()
```

That lets your packet parsing and application behavior be identical while only the radio transport changes.

If you really need to test actual ESP-NOW behavior, I think the cheapest and least painful solution is actually a second ESP32-C3 sitting permanently at your desk.

Something like:

```text
PC
 │ USB
 ▼
spare XIAO ESP32-C3
 │
 ├── real Wi-Fi
 ├── real ESP-NOW
 ├── real SPI
 └── real RMT
       │
       ▼
 logic-capture / simulator adapter
```

The board is tiny and inexpensive, and it gives you the real ESP-IDF Wi-Fi stack, real RF hardware, real SPI peripheral, real RMT peripheral and real DMA behavior.

You don't even need another physical display. Connect the spare ESP's SPI/latch/OE/row lines to a cheap USB logic analyzer, or to another microcontroller/RP2040 that acts as a “display emulator.” Your PC can then render what that adapter receives as a 16×16 grid.

For example:

```text
             USB
              │
         ESP32-C3 dev
              │
     DATA CLK LAT OE ROW
              │
        RP2040 / Pico
              │ USB serial
              ▼
       PC display viewer
       
      ┌─────────────────┐
      │ □ ■ ■ □ ...     │
      │ ■ □ □ ■ ...     │
      │ ■ ■ ■ ■ ...     │
      │ ...             │
      └─────────────────┘
```

That solution is especially good if you're debugging timing bugs, because the signal generation is still happening on the actual ESP hardware.

So I'd use three levels of testing:

1. Host simulator for most development. Compile your display logic for Linux/macOS/Windows and render the 16×16 framebuffer using SDL, terminal graphics, a tiny web UI, etc. Mock ESP-NOW with UDP. This will probably cover 80–90% of development work.

2. Wokwi or ESP32-C3 emulation for firmware-level checks. Useful for booting the actual ESP-IDF image, FreeRTOS behavior, Wi-Fi/application integration and some peripheral behavior. Wokwi is particularly convenient because it explicitly supports XIAO ESP32-C3, SPI and Wi-Fi. ([Wokwi Docs][3])

3. A €5–10 spare XIAO ESP32-C3 on your desk for hardware-in-the-loop. Use it whenever you're working on SPI DMA, RMT timing, ESP-NOW, ISR behavior, or synchronization between those systems.

I would not make “capture SPI/RMT directly from QEMU” the primary architecture. You could extend Espressif's QEMU/emulator and add hooks for those peripherals, but you're effectively writing peripheral models at that point. Unless you're interested in emulator development itself, the effort probably exceeds the value for a 16×16 display.

One particularly clean trick for your firmware would be to make the simulator a normal ESP-IDF build target choice:

```c
#ifdef CONFIG_DISPLAY_BACKEND_HW

#include "display_backend_esp.c"

#elif CONFIG_DISPLAY_BACKEND_SIM

#include "display_backend_sim.c"

#endif
```

Or better, have two components:

```text
components/
    display/
        display.c
        bitplane.c
        include/display.h

    display_hw/
        display_hw.c       // SPI/RMT

    display_sim/
        display_sim.c      // socket/frame output
```

Then the simulator could emit very simple messages such as:

```text
FRAME
0000000000000000
0000111111110000
0011000000001100
...
```

or a packed 32-byte framebuffer over a socket.

A tiny Python/SDL/web viewer can then scale each “pixel” to, say, a 30×30 rectangle. You could additionally emulate the actual bitplane timing rather than just displaying the final framebuffer, which would make PWM/brightness/scan artifacts visible too.

For your particular project, I'd choose the host-simulator + spare ESP32-C3 combination. It gives you very fast development without compromising the things that actually need real silicon. QEMU/Wokwi can be useful additions, but I wouldn't make either one the foundation of the testing setup.

[1]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/qemu.html?utm_source=chatgpt.com "QEMU Emulator - ESP32 - — ESP-IDF Programming Guide v6.1 documentation"
[2]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/host-apps.html?utm_source=chatgpt.com "Running ESP-IDF Applications on Host - ESP32 - — ESP-IDF Programming Guide latest documentation"
[3]: https://docs.wokwi.com/guides/esp32?utm_source=chatgpt.com "ESP32 Simulation | Wokwi Docs"

