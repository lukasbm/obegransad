# obegransad

IKEA obegränsad hack

## TODO

- [ ] Brightness does not do anything.
- [ ] Errors, e.g. with WiFi are not handled gracefully.
- [ ] NTP is synced very infrequently!

#### Graphics

- [ ] Clear (sonne oder mond, je nach tageszeit) [0]
- [ ] Bewölkt (eine wolke) [1,2,3,45,48]
- [ ] Regen (wolke mit regentropfen, könnte auch animiert sein...)
- [ ] 

Ich glaube das compositing sollte per code stattfinden.
Also falls freezing, dann oben links eine schneeflocke.

Wetter design: obere hälfte symbol, unten temperatur.

Komponenten:
- Wolke.bmp
- Sonne.bmp
- Regen.bmp (could be a animation spritesheet)
- Freezing.bmp (als indicator)
- Schnee.bmp (falling out of cloud, animation)
- blitz.bmp (falls thunderstorm)

## Hardware

### RTC Clock

The ESP32 has an RTC:

- A low-frequency RTC timer that can wake the chip from light- or deep-sleep with microsecond precision.
- An 8 KB SRAM block (RTC fast memory) for retaining data across deep-sleep cycles.
- A calendar “wall-clock” when you sync via SNTP (persisted by the RTC timer across soft-resets and deep-sleeps, but not across full power-on resets).

### Graphics

Graphics are drawn as bitmaps (e.g. using Pixelorama, Piskel, LibreSprite or [Tilesetter](https://www.tilesetter.org/)) and the converted to C++ arrays using an online tool called [img2cpp](https://hurricanejoef.github.io/image2cpp/).

## Usage

### Setup

The screen will open a captive portal to request the WiFi credentials.
Just connect to the `Obegransad` wifi device with your phone.
If the portal does not open automatically, enter the IP `192.168.4.1` in your browser.
