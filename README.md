# Obegransad

IKEA obegränsad hack.

This repository contains the code for a much extended firmware over existing snippets and sketches.

## Features

- Full WiFI Captive portal for setup
- Web Interface and API for configuration
- Many screensavers (some replicated from the original unmodded obegransad)
- Many scenes
    - 3 Weather Scenes (with animations)
    - Moon Phase scene
    - 2 Clock scene (with animations)
    - Anniversary Scene (with animations)

## Build

Please see the original guide by [Digital Image](http://blog.digital-image.de/2023/05/31/x-clock/).

This repository only provides the code for a much much much extended firmware compared to the one described above.

### Pin Connection

Contrary to the guides ESP8266, this repo uses an (XIAO) ESP32 (C3) with 400kb SRAM and 4MB flash, as well as WiFi.
This means we use the following pins:
- LATCH =  D1
- CLK = D2
- DI = D3
- OE = D4
- Button = D7
- And naturally 5V (VUSB) and GND.

## Usage

### Setup

On initial setup the device will open a captive portal to request the WiFi credentials.
Just connect to the `Obegransad-Setup` wifi network with your phone.
If the portal does not open automatically, enter the IP `192.168.4.1` in your browser.

### Config

Either use the web interface or the API (`http://192.168.4.1/api/settings`)
Look at `example_config.json` for possible configs.

The time zones are used as described here: <https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv>

To bring back the captive portal hold the button for at least 5 seconds.

You can also use the captive portal to fully wipe the settings from the NVS.

### Scenes

Pressing the button once switches to the next scene.

As mentioned in the config there is also a "digital frame"-mode which periodically switches scenes.

## Technical Information for Developers

See [DEVELOPERS.md](DEVELOPERS.md).
