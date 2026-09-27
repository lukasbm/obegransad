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

**What was tried (all flaky, no fix found)**

- `-nic` vs `-netdev`/`-device` style: `-netdev ... -device open_eth` never
  worked at all (0/5); `-nic user,model=open_eth` is the only usable form.
- Serial/monitor setup: `mon:stdio`, `-serial file:...`, TCP serial — all
  reproduce it.
- `-accel tcg,thread=single`, `ipv6=off`, explicit guest IP in `hostfwd`,
  CPU warm-up before the run, `--persist-flash` vs plain runs: all stayed
  flaky (0/5–4/5 depending on the batch).
- Host libslirp: this QEMU build links the *system* `libslirp.so.0`
  (4.9.1 on Fedora 44). An interleaved A/B against Fedora 42's 4.8.0 gave
  4/6 vs 2/6 — suggestive but not conclusive with this much variance.

**Root cause**: not pinned down. The Espressif QEMU build has no slirp
tracepoints compiled in, so slirp internals cannot be observed without
rebuilding QEMU. The failure is per QEMU process: a run either works for all
connections or none.

**Fixes / workarounds**

- Retry the run (`tools/sim/ota-test.sh` does this automatically).
- Pull-based OTA (device downloads from a URL) instead of push: guest→host
  traffic is reliable, so it sidesteps the flake entirely — and it is useful on
  hardware anyway (release URLs). Not implemented yet (phase 2 in
  ota_plan.md).
- Tap networking: the guest gets a real LAN address, `hostfwd` is bypassed
  entirely. Needs root; the deterministic option.
- Build QEMU from source with a bundled/debug slirp if this ever becomes a
  real blocker.

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
next run, which is why an OTA'd slot does not normally survive a simulator
restart — use `tools/sim/run-qemu.sh --persist-flash` to keep the whole image
(code, NVS, `otadata`, both slots). Delete `build-sim/qemu_flash_full.bin` to
start fresh.

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
- **Host build returns 501** (no flash/partitions). In QEMU, `--persist`
  preserves only the NVS region; use `--persist-flash` to keep the whole image
  including `otadata` and both slots, so OTA updates survive simulator
  restarts (verified by booting a patched `otadata` twice).
- **Wired reflash**: use `tools/flash.sh --port PORT`; it flashes and then
  erases `otadata` so the freshly flashed `ota_0` boots (plain `idf.py flash`
  does not touch `otadata`). NVS is kept unless `--erase-nvs` is given.

## Partition layout

Current table: `nvs` 24 KiB, `phy_init` 4 KiB, `otadata` 8 KiB, `ota_0` and
`ota_1` 1984 KiB each — it fills the 4 MB flash exactly.

- **No `factory` partition**: with blank `otadata` the bootloader boots `ota_0`
  ("No factory image, trying OTA 0"). Works, but `idf.py flash` always targets
  `ota_0` and leaves `otadata` alone — use `tools/flash.sh` (see README).
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
