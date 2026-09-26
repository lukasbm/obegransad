# Simulator (QEMU)

Runs the real firmware on an emulated ESP32-C3 with the display/button replaced
by simulator backends. FreeRTOS, `esp_timer`, `esp_event`, NVS, lwIP, SNTP and
the HTTP/TLS stack are real; SPI/RMT/GPIO and the `esp_wifi` driver are not
(they are not emulated by QEMU — see "Limitations").

## Run

```sh
tools/sim/run-qemu.sh              # renderer window + idf.py monitor
tools/sim/run-qemu.sh --persist    # keep NVS/config across runs
tools/sim/run-qemu.sh --no-monitor # QEMU in the foreground, no monitor
```

Options: `--sim-port N` (default 5566), `--http-port N` (default 8080,
`0` disables), `--persist` (keep NVS), `--fresh` (regenerate
`build-sim/sdkconfig` from the defaults files), `--no-renderer`, `--no-monitor`.

The script uses `build-sim/` and `build-sim/sdkconfig` and layers
`sdkconfig.defaults[.local]` + `sdkconfig.defaults.sim`; the hardware build and
its `sdkconfig` are untouched.

## Renderer

`tools/sim/renderer.py` listens on TCP port 5566 (the firmware connects to it,
so start order does not matter). `run-qemu.sh` binds the renderer to `0.0.0.0`
because QEMU's slirp connects from the host interface address rather than
loopback; a host build can use the default `--host 127.0.0.1`. It draws the
16x16 panel and sends button commands back:

| Input | Action |
|---|---|
| `s` / Short | `APP_EVT_BUTTON_SHORT` (next preset) |
| `d` / Double | `APP_EVT_BUTTON_DOUBLE` (previous preset) |
| `l` / Long | `APP_EVT_BUTTON_LONG` (factory reset + restart) |

Headless text mode (SSH, scripts):

```sh
python3 tools/sim/renderer.py --ascii
```

## Networking

QEMU provides emulated OpenCores Ethernet with slirp: DHCP, DNS and NAT to the
host network. The simulator device backend (`main/device_sim.cpp`) brings it up
instead of `esp_wifi` and posts the normal `APP_EVT_WIFI_CONNECTED`, so
weather/HTTPS/NTP and any future HTTP/MQTT server work unchanged.

* Outbound (weather, NTP, Home Assistant REST/MQTT): works through the host.
* Inbound: `http://127.0.0.1:8080/` is forwarded to guest port 80. Change the
  host port with `--http-port`.

Wi-Fi credentials from `sdkconfig.defaults.local` are compiled in but ignored.
To exercise the `SETUP` state, enable `CONFIG_OBG_SIM_FAKE_NO_CREDS`; the
captive portal itself cannot run without real AP hardware.

## Limitations

| Feature | QEMU |
|---|---|
| FreeRTOS, esp_timer, esp_event, NVS | real |
| Display | framebuffer sent to the renderer (no SPI/RMT) |
| Buttons | renderer keys (GPIO input is not emulated) |
| Wi-Fi / captive portal (AP) | not emulated → emulated Ethernet instead |
| `esp_restart()` on long press | may not reset the emulated chip; restart the script if it hangs |

For real SPI/RMT timing and Wi-Fi behaviour, test the hardware build; the
renderer cannot reproduce panel timing artifacts.
