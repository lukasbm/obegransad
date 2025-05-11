# obegransad

IKEA obegränsad hack

## TODO

- [X] Brightness does not do anything.
- [ ] Errors, e.g. with WiFi are not handled gracefully.
- [ ] NTP is synced very infrequently!
- [ ] Would be nice if i didn't have to way on the weatherData. This would mean caching and refreshing it outside the scene, e.g. the Weather module or a new Data Access module that also takes care of other things. Alternatively the "conduct_checks" loop could update the weather data

## Hardware

### RTC Clock

The ESP32 has an RTC:

- A low-frequency RTC timer that can wake the chip from light- or deep-sleep with microsecond precision.
- An 8 KB SRAM block (RTC fast memory) for retaining data across deep-sleep cycles.
- A calendar “wall-clock” when you sync via SNTP (persisted by the RTC timer across soft-resets and deep-sleeps, but not across full power-on resets).

## Usage

### Setup

The screen will open a captive portal to request the WiFi credentials.
Just connect to the `Obegransad` wifi device with your phone.
If the portal does not open automatically, enter the IP `192.168.4.1` in your browser.

### Config

Look at `example_config.json` for possible configs.
The time zones are used as described here: <https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv>
