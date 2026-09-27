# Simulator

Two ways to run the firmware without the board, both replacing the display
output, the button and the Wi-Fi stack with simulator backends while the rest
of the app (state machine, scenes, NVS, `esp_event`, the HTTP/TLS stack) runs
unchanged:

| | `run-qemu.sh` | `run-host.sh` |
|---|---|---|
| Execution | real ESP32-C3 image under QEMU | native Linux process (`target linux`) |
| FreeRTOS/`esp_timer` | real (emulated chip) | POSIX simulator + `esp_timer` shim |
| Network | emulated Ethernet + slirp NAT | host network stack directly |
| Best for | firmware-level checks, target behaviour | fast scene/server iteration, debuggers |

## QEMU

```sh
tools/sim/run-qemu.sh              # renderer window + idf.py monitor
tools/sim/run-qemu.sh --persist    # keep NVS/config across runs
tools/sim/run-qemu.sh --persist-flash  # keep the whole flash (incl. OTA slots)
tools/sim/run-qemu.sh --no-monitor # QEMU in the foreground, no monitor
tools/sim/ota-test.sh              # push an image and verify the slot switch
```

Options: `--sim-port N` (default 5566), `--http-port N` (default 8080,
`0` disables), `--persist` (keep NVS), `--persist-flash` (keep the whole flash
image; delete `build-sim/qemu_flash_full.bin` to start fresh), `--fresh`
(regenerate `build-sim/sdkconfig` from the defaults files), `--no-renderer`,
`--no-monitor`.

The script uses `build-sim/` and `build-sim/sdkconfig` and layers
`sdkconfig.defaults[.local]` + `sdkconfig.defaults.sim`; the hardware build and
its `sdkconfig` are untouched.

## Native Linux host

```sh
tools/sim/run-host.sh              # renderer window, app runs natively
tools/sim/run-host.sh --fresh      # regenerate build-host/sdkconfig
```

**Requirement:** the `libbsd` development headers (IDF's linux target includes
`bsd/sys/cdefs.h`). Fedora: `sudo dnf install libbsd-devel`; Debian/Ubuntu:
`sudo apt install libbsd-dev`. Without root, extract the package and point
`OBG_LIBBSD_PREFIX` at the prefix.

The target is a preview/experimental ESP-IDF feature. The runner sets
`IDF_TARGET=linux` and uses `build-host/`; the root `CMakeLists.txt` restricts
the build to `main` and its dependencies (required by IDF for this target).

Differences from QEMU worth knowing:

* No emulated hardware at all: no flash image, no real `esp_timer` (a small
  shim in `components/obegransad-host/` provides it, so sub-tick periods are
  rounded up to one FreeRTOS tick), no real ISR dispatch.
* The host network stack is used directly: the app's HTTP server binds host
  ports (no forwarding), and outbound HA/MQTT/weather use normal sockets/DNS.
* `psa_crypto_init()` is called at startup; without it the first TLS handshake
  fails on this target.
* Ideal for native `gdb`, sanitizers and fast restarts; not a substitute for
  target timing or RF behaviour.

## Renderer

`tools/sim/renderer.py` listens on TCP port 5566 (the firmware connects to it,
so start order does not matter). `run-qemu.sh` binds the renderer to `0.0.0.0`
because QEMU's slirp connects from the host interface address rather than
loopback; the host build uses the default `127.0.0.1`. It draws the 16x16 panel
and sends button commands back:

| Input | Action |
|---|---|
| `s` / Short | `APP_EVT_BUTTON_SHORT` (next preset) |
| `d` / Double | `APP_EVT_BUTTON_DOUBLE` (previous preset) |
| `l` / Long | `APP_EVT_BUTTON_LONG` (factory reset + restart) |

Headless text mode (SSH, scripts):

```sh
python3 tools/sim/renderer.py --ascii
```

## Stopping

- **QEMU, default (with monitor):** press `Ctrl-]` in the terminal. The monitor
  exits and `idf.py` then terminates QEMU.
- **QEMU `--no-monitor`:** press `Ctrl-a` then `x` (QEMU's own exit), or
  `Ctrl-C`.
- **Native host build:** `Ctrl-C` in the terminal.
- Closing the renderer window does **not** stop the firmware; stop it from the
  terminal.

If a terminal is gone or the simulator seems stuck, from another terminal:

```sh
pkill -f '[q]emu-system-riscv32'    # QEMU
pkill -f '[t]ools/sim/renderer.py'  # renderer window
```

(The brackets stop the commands from matching themselves.) Note that after
boot the logs are quiet and preset 0 "Calm" is a static clock that only
redraws at minute boundaries — an unchanging picture is expected. Press `s` in
the renderer window (click it first) or use the preset buttons on
<http://127.0.0.1:8080/> to rotate scenes.

## Where the scenes and presets live

- `main/presets.hpp` — the ten presets: name, dwell (`0` = never rotate) and
  the ordered list of scene names. The list order *is* the rotation.
- `main/scene_registry.hpp` — which scene objects are instantiated and
  registered.
- `main/scenes/*.hpp` — the scenes themselves (`render()`, `target_fps()`,
  `requires_wifi()`, `get_scene_name()`).
- `main/scene.h` — the `Scene` base class and the authoring contract.
- Rotation logic: `main/scene_switcher.cpp` and `StateMachine::update()`
  (`main/state.cpp`).
- At runtime: `GET /api/info` lists every preset and scene; the settings page
  on <http://127.0.0.1:8080/> has preset/scene buttons.

## Networking

QEMU provides emulated OpenCores Ethernet with slirp: DHCP, DNS and NAT to the
host network. The simulator device backend (`main/device_sim.cpp`) brings it up
instead of `esp_wifi` and posts the normal `APP_EVT_WIFI_CONNECTED`, so
weather/HTTPS/NTP and any future HTTP/MQTT server work unchanged. The host
build (`main/device_host.cpp`) announces the same event using the host stack.

* Outbound (weather, NTP, Home Assistant REST/MQTT): works in both.
* Inbound under QEMU: `http://127.0.0.1:8080/` is forwarded to guest port 8080
  (`--http-port`). On the host build the server binds host port 8080 directly.
* OTA: `POST /api/ota` with a raw `obegransad.bin` writes the inactive slot and
  restarts (`X-OTA-Token` header required; `tools/ota-push.sh` and
  `tools/sim/ota-test.sh` read the token from `sdkconfig.defaults.local`).
  `tools/sim/ota-test.sh` does this end-to-end under QEMU (boot, upload,
  verify the slot switch). Within one QEMU run the emulated flash and
  `esp_restart()` are real; `--persist-flash` keeps the result across runs.

**Known QEMU quirk:** user-mode host forwarding is occasionally unable to
deliver *inbound* connections (the host socket connects, slirp never forwards
the request). Outbound works, and a run that starts working stays working.
Retry the run if `curl` hangs; `ota-test.sh` does that automatically.

Wi-Fi credentials from `sdkconfig.defaults.local` are compiled in but ignored
by the simulator backends. There is no provisioning portal and no SETUP state
(both removed, see `docs/known-issues.md`).

## Limitations

| Feature | QEMU | Host |
|---|---|---|
| FreeRTOS, `esp_event`, NVS | real | POSIX simulator |
| `esp_timer` | real | shim (tick-granular) |
| Display | framebuffer to the renderer (no SPI/RMT) | same |
| Buttons | renderer keys | renderer keys |
| Wi-Fi (driver) | emulated Ethernet instead | host stack |
| `esp_restart()` on long press | may not reset the emulated chip; restart the script if it hangs | same |

For real SPI/RMT timing, Wi-Fi behaviour and provisioning, test the hardware
build; the renderer cannot reproduce panel timing artifacts.
