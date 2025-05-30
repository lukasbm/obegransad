# obegransad

IKEA obegränsad hack

## TODO

- [X] Brightness does not do anything.
- [ ] Would be nice if i didn't have to wait on the weatherData. This would mean caching and refreshing it outside the scene, e.g. the Weather module or a new Data Access module that also takes care of other things. Alternatively the "conduct_checks" loop could update the weather data
- [ ] Redesign the weather scene. Instead it should show the max temperature in bold (on the top), the min temperature (bottom left) and maybe an icon in the bottom right.
    Alternatively have two weather scenes. One with the numbers and one with the icons.
- [ ] Add more animations/screensaver style scenes.
- [ ] Refactor scenes into one file each!
- [ ] How to handle WiFi outages? Maybe need a general error screen that replaces weather and other wifi dependent stuff
- [ ] also need to introduce error code in case some setup stuff goes wrong
- [ ] make it also usable without WiFi. e.g. by pressing the button while in AP mode!
- [ ] a simple HTML file that the user can copy to any device that contains a UI for the configuration. Ideally that file is automatically generated based on a json-schema of the config. Then the form inputs are submitted to the ESP for update!
    Alternatively make the asyncHTTPserver provide it (maybe not dynamically, that might be slow!)

## Discussion

## How to handle WiFi

Ideally the thing also works offline. In that mode scenes like weather do not work.

### Force Setup at beginning

Would not make much sense as an intermediate connection loss will result in the same problem as not setting up initially.

The loop and some function need to be adapted.
E.g. weather and NTP updater should not run if there is no WiFI (timeout takes too long)

### Don't Force Setup

Problem: things like the clock might be completely off at the beginning without wifi.
So to make it work if there was no initial wifi for the setup, we need to be able to access the config page before we even have an IP and can host the config server. The only way to do this is by extending the captive portal (or rolling our own).

### Network Change?

Either hold down the button for a while (5 seconds) this either clears the WiFi credentials or re-opens the captive portal.

### Custom Captive Portal

Takes care of Settings AND wifi. should be invoked by long button press!
Also gives me the "easy" option to store the settings as well as the WiFi credentials in NVM.
While the captive portal is open, nothing else should be displayed on screen!

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
