# Known issues and limitations

Living list of caveats around the simulator, OTA and the partition layout.
See also `tools/sim/README.md` (simulator usage), `docs/openapi.yaml` (API)
and `ota_plan.md` (the original OTA design document).

## Captive portal and SETUP state removed

The provisioning AP/web UI from `78/esp-wifi-connect` (`WifiConfigurationAp`)
and the `SETUP` state were removed. The portal ran a second `esp_http_server`
on port 80 with control port 32768, which clashed with the config server:

- the UDP control socket is bound without `SO_REUSEADDR`, so the second
  `httpd_start()` fails (`ESP_FAIL`) and the `ESP_ERROR_CHECK` aborts, and
- both servers on TCP port 80 make one of the two UIs silently unreachable.

The config server is now the device's only HTTP server (port 8080, control port
32769), and the whole `78/esp-wifi-connect` dependency is gone: credentials are
read from Kconfig and kept in RAM by the small station driver in
`main/device_hw.cpp`.

**Consequences**

- Wi-Fi credentials come from Kconfig (`CONFIG_OBG_WIFI_SSID`/`_PASSWORD`,
  normally in the gitignored `sdkconfig.defaults.local`). There is no in-field
  provisioning path right now.
- With no credentials the device logs an error and stays in `DEGRADED`
  (offline scenes) instead of entering a provisioning mode.
- The long-press button action (factory reset) is a **no-op**: it was the only
  path that cleared credentials, and without a provisioning mode it would have
  left the device offline until reflashed. The button still emits
  `APP_EVT_BUTTON_LONG` (visible via `/api/events`), nothing acts on it.

**Possible follow-ups**: expose Wi-Fi settings on the config server (only
reachable while connected) or add BLE/AP provisioning later.

## QEMU user-mode networking: flaky inbound host forwarding

Host→guest connections through `-nic user,...,hostfwd=...` fail in a random
fraction of QEMU runs (observed anywhere from 0/3 to 4/4 in batches): QEMU
accepts the host connection, the request stays unread in its socket queue, and
slirp never forwards it to the guest. Outbound traffic (guest→internet,
guest→host renderer) keeps working, and a run that starts working stays
working.

**What happens, step by step**

1. `hostfwd` makes QEMU's user-mode network (slirp) listen on the host port and
   accept a connection there. That part always works — the host sees
   `ESTABLISHED` against the QEMU process.
2. Slirp is then supposed to open a matching TCP connection *inside* the
   virtual network, i.e. send a SYN to the guest IP (`10.0.2.15:8080`).
3. In failing runs that SYN never reaches the guest (or its reply never comes
   back), so the connection stalls. The request bytes are visible as unread
   data in QEMU's receive queue:

   ```
   ESTAB 85 0 127.0.0.1:8080 127.0.0.1:49960 users:(("qemu-system-ris",fd=10))
   ```

4. Retrying the whole QEMU process fixes it; retrying the *connection* inside
   the same run does not, so the failure is per-run state, not per-connection.

**What it is not**: not the guest HTTP server (only one runs now), not the
serial/monitor setup (`mon:stdio`, file, TCP — all reproduce it), not
`-nic`/`-netdev` style or port numbers, and not host CPU load. It is also
unrelated to the old captive portal conflict (that was two servers inside the
guest).

**Root cause**: not pinned down. The Espressif QEMU build has no slirp
tracepoints compiled in, and the lwIP debug options did not produce per-packet
logs, so the guest-side handshake could not be observed. It looks like a
slirp-side race around the guest connection (plausibly ARP/route resolution for
the forwarded connection).

**Workarounds**: retry the run (`tools/sim/ota-test.sh` does this
automatically). The renderer and button input use an outbound connection and
are unaffected. To fix it properly: build QEMU from source with slirp debug,
try another QEMU version, or switch to tap networking (needs root; the guest
gets a real LAN address and hostfwd is bypassed).

## OTA

**How it works (one breath)**: the device has two app slots; `otadata` tells
the bootloader which one to run. `POST /api/ota` writes the uploaded image into
the *inactive* slot in 4 KiB chunks while the current image keeps running,
`esp_ota_end()` verifies the image structure, and
`esp_ota_set_boot_partition()` writes `otadata` to point at the new slot; the
device then restarts into it. With rollback enabled the new image starts as
`PENDING_VERIFY` and `ota_init()` confirms it early, so a reset before
confirmation rolls back. Because `otadata` is only touched at the very end, an
interrupted upload or power loss is harmless.

In QEMU the flash file persists across the guest's `esp_restart()`, so a full
cycle works within one run. `idf.py qemu` regenerates the flash image on the
next run, which is why an OTA'd slot does not survive a restart of the
simulator (`--persist` only restores the NVS region).

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

## Dependencies

`dependencies.lock` is not tracked. The component manager keeps one lock file
per project, but the Linux target resolves a different dependency set (the
manifest rules in `main/idf_component.yml` can exclude components per target),
so the file would flip-flop between chip and host builds. Versions are pinned
in `main/idf_component.yml`; `managed_components/` is regenerated on demand.
