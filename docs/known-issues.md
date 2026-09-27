# Known issues and limitations

Living list of caveats around the simulator, OTA and the partition layout.
See also `tools/sim/README.md` (simulator usage), `docs/openapi.yaml` (API)
and `ota_plan.md` (the original OTA design document).

## Captive portal removed

The provisioning AP/web UI from `78/esp-wifi-connect`
(`WifiConfigurationAp`) was removed. It ran a second `esp_http_server` on port
80 with control port 32768, which clashed with the config server:

- the UDP control socket is bound without `SO_REUSEADDR`, so the second
  `httpd_start()` fails (`ESP_FAIL`) and the `ESP_ERROR_CHECK` aborts, and
- both servers on TCP port 80 make one of the two UIs silently unreachable
  (`SO_REUSEADDR` lets both binds succeed).

The config server is now the only HTTP server (port 8080, control port 32769).

**Consequences**

- Wi-Fi credentials come from Kconfig (`CONFIG_OBG_WIFI_SSID`/`_PASSWORD`,
  normally in the gitignored `sdkconfig.defaults.local`) or from NVS seeded
  earlier. There is no in-field provisioning path right now.
- After a factory reset (long press clears credentials and restarts) the device
  boots into `SETUP` and stays offline until it is reflashed with credentials.
  This is the main open UX gap of the removal.
- `78/esp-wifi-connect` is still a dependency (for `SsidManager` and
  `WifiStation`), so its unused portal assets still take flash space.

**Possible follow-ups**: expose Wi-Fi settings on the config server (only
reachable while connected), add BLE/AP provisioning later, or make the long
press not clear credentials.

## QEMU user-mode networking: flaky inbound host forwarding

Host→guest connections through `-nic user,...,hostfwd=...` fail in roughly
30–40 % of QEMU runs: QEMU accepts the host connection, the request stays
unread in its socket queue, and slirp never forwards it to the guest. Outbound
traffic (guest→internet, guest→host renderer) keeps working, and a run that
starts working stays working. Reproduced with the Espressif QEMU 9.2.2 fork on
Fedora 44; independent of the serial/monitor setup and of `-nic`/`-netdev`
style.

**Workarounds**: retry the whole run (`tools/sim/ota-test.sh` does this); the
renderer and button input use an outbound connection and are unaffected.

## OTA

- **No authentication**: `POST /api/ota` is remote code execution for anyone on
  the LAN. It is refused unless the station is connected (403), but there is no
  token, TLS or physical-presence check.
- **No image provenance check**: `esp_ota_end()` validates the image structure,
  not that it belongs to this project. Only upload bins built from this repo.
- **No version policy**: flashing the same or an older image is allowed. There
  is no anti-rollback (eFuse-based, irreversible) and no version manifest.
- **Rollback is confirmed immediately** in `ota_init()`. An image that boots but
  fails later will not roll back; confirming after Wi-Fi connects or after a
  health delay would be stricter.
- **No progress in `GET /api/ota` during an upload**: `esp_http_server` is
  single-task, so the status endpoint cannot be served while receiving.
- **Host build returns 501** (no flash/partitions). QEMU works within one run,
  but `--persist` preserves only the NVS region, so an OTA'd slot does not
  survive a restart of `idf.py qemu`. Cross-run OTA testing would need a
  "keep the whole flash image" option.
- **Partition migration is one-time and wired** (see README): flash the new
  table, then `erase-region 0x10000 0x2000` to clear stale `otadata`. A later
  wired `idf.py flash` always writes `ota_0`, but if `otadata` still points at
  `ota_1` the device keeps booting the OTA image; erase `otadata` again to boot
  the freshly flashed one.

## Partition layout

Current table: `nvs` 24 KiB, `phy_init` 4 KiB, `otadata` 8 KiB, `ota_0` and
`ota_1` 1984 KiB each — it fills the 4 MB flash exactly.

- **No `factory` partition**: with blank `otadata` the bootloader boots `ota_0`
  ("No factory image, trying OTA 0"). Works, but `idf.py flash` always targets
  `ota_0`.
- **Headroom**: the hardware image is ~1.22 MiB, so each slot has ~37 % free.
  Large additions (a second TLS stack, Matter, big assets) can exceed the slot;
  watch the build's "Smallest app partition" line.
- **No data/SPIFFS partition**: the `data/` assets are embedded in the app.
- **24 KiB NVS** is enough for the current config; revisit if more state is
  stored there.
- Alternatives rejected in `ota_plan.md`: `factory` + one slot (no rollback),
  `factory` + two slots (too little headroom), the built-in two-OTA table
  (3 × 1 MiB — the image does not fit). An 8/16 MB flash part would be the way
  to gain room.

## Simulator

- QEMU emulates no Wi-Fi/BLE/AP; networking is emulated Ethernet.
- No SPI/RMT/GPIO timing fidelity: panel output is a framebuffer over TCP.
- The native host (`linux`) target has no flash/partitions: OTA returns 501 and
  NVS is host-side.
- `idf.py qemu` regenerates `qemu_flash.bin` on every run, so NVS and `otadata`
  reset unless `--persist` (NVS only) is used.
- Renderer keys are `s`/`d`/`l`; everything else goes through the HTTP API on
  port 8080.
