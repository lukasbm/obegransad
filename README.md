# obegransad

IKEA obegränsad hack

## TODO

- [ ] Brightness does not do anything.
- [ ] Errors, e.g. with WiFi are not handled gracefully.
- [ ] NTP is synced very infrequently!

## Hardware

### RTC Clock

The ESP32 has an RTC:

- A low-frequency RTC timer that can wake the chip from light- or deep-sleep with microsecond precision.
- An 8 KB SRAM block (RTC fast memory) for retaining data across deep-sleep cycles.
- A calendar “wall-clock” when you sync via SNTP (persisted by the RTC timer across soft-resets and deep-sleeps, but not across full power-on resets).
