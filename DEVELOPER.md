# Developer Guide

## TODO

- [X] Brightness does not do anything.
- [ ] Would be nice if i didn't have to wait on the weatherData. This would mean caching and refreshing it outside the scene, e.g. the Weather module or a new Data Access module that also takes care of other things. Alternatively the "conduct_checks" loop could update the weather data
- [ ] Redesign the weather scene. Instead it should show the max temperature in bold (on the top), the min temperature (bottom left) and maybe an icon in the bottom right.
    Alternatively have two weather scenes. One with the numbers and one with the icons.
- [x] Add more animations/screensaver style scenes.
- [x] Refactor scenes into one file each!
- [ ] How to handle WiFi outages? Maybe need a general error screen that replaces weather and other wifi dependent stuff
- [ ] also need to introduce error code in case some setup stuff goes wrong
- [ ] make it also usable without WiFi. e.g. by pressing the button while in AP mode!
- [ ] a simple HTML file that the user can copy to any device that contains a UI for the configuration. Ideally that file is automatically generated based on a json-schema of the config. Then the form inputs are submitted to the ESP for update!
    Alternatively make the asyncHTTPserver provide it (maybe not dynamically, that might be slow!)

## Discussion

## How to handle WiFi

Ideally the thing also works offline. In that mode scenes like weather do not work.

## Hardware

### Firmware Update

```bash
# first compile the assets
cd src/portal
./compress.sh
cd ../.. 
# flash assets
pio run -t uploadfs
# build and flash firmware
pio run -t build
pio run -t upload
# monitor
pio run -t monitor
```

### RTC Clock

The ESP32 has an RTC:

- A low-frequency RTC timer that can wake the chip from light- or deep-sleep with microsecond precision.
- An 8 KB SRAM block (RTC fast memory) for retaining data across deep-sleep cycles.
- A calendar “wall-clock” when you sync via SNTP (persisted by the RTC timer across soft-resets and deep-sleeps, but not across full power-on resets).
