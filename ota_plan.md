# OTA update plan (Wi-Fi, push via the config server)

Status: proposed, not implemented. Target: ESP32-C3 board, 4 MB flash, ESP-IDF v6.0.1.

This plan adds over-the-air firmware updates to the hardware build. Updates are
pushed to the device over the existing REST config server (`main/config_server.cpp`,
contract in [[docs/openapi.yaml]]) — the device needs no server of its own and no
change to the network setup beyond pointing a browser/curl at it.

Push (device receives the image) is chosen for the first version because:

- it reuses the config server that is being added anyway (one small module,
  size estimate in section 9),
- it needs no hosting infrastructure and no polling logic,
- the device is on the home LAN, so it is reachable,
- updates are an explicit, supervised action (button/curl), which matches how the
  device is used.

Pull OTA (`esp_https_ota`, device downloads from a URL) is documented as an
optional phase 2 in section 9. The captive portal component already stores a
`ota_url` key in NVS (`wifi` namespace) that nothing uses yet — that is the
natural trigger for it.

---

## 1. Current state (measured from `build/`)

| Artifact | Size |
|---|---|
| `build/obegransad.bin` | 1,240,656 B (0x12EE50) = 1.18 MiB |
| `build/bootloader/bootloader.bin` | 21,136 B (0x5290) |
| `build/partition_table/partition-table.bin` | 3,072 B |
| Total flashed by `idf.py flash` | ~1.21 MiB of 4 MiB |

Current `partitions.csv` has a single 3 MiB `factory` partition, no `otadata`,
no second app slot: OTA is impossible until the table changes (section 3).
For reference, the image is 1.18 MiB, so a two-slot layout has plenty of room.

## 2. What changes at a glance

| Area | Change |
|---|---|
| `partitions.csv` | `otadata` + `ota_0` + `ota_1` (2 × 1984 KiB), no `factory` |
| `sdkconfig.defaults` | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` (recommended) |
| `main/CMakeLists.txt` | add `app_update` to `reqs` (phase 2: `esp_https_ota`) |
| new `main/ota.{h,cpp}` | upload/verify/apply logic, status, delayed restart |
| `main/config_server.cpp` | register `GET`/`POST /api/ota` |
| `main/main.cpp` | `ota_init()` at boot, `ota_process()` in the main loop |
| `docs/openapi.yaml` | document `/api/ota` (snippet in section 7) |

No change is needed to `CONFIG_ESPTOOLPY_FLASHSIZE_4MB` or the custom partition
table wiring (`CONFIG_PARTITION_TABLE_CUSTOM` is already set in
`sdkconfig.defaults.esp32c3`), and no other sdkconfig changes are required.

## 3. Partition table

Replace the body of `partitions.csv` with:

```csv
# ESP-IDF Partition Table
# Name                   , Type, SubType, Offset  , Size    , Flags
nvs                      , data, nvs    , 0x9000  , 0x6000  ,
phy_init                 , data, phy    , 0xf000  , 0x1000  ,
otadata                  , data, ota    , 0x10000 , 0x2000  ,
ota_0                    , app , ota_0  , 0x20000 , 0x1F0000,
ota_1                    , app , ota_1  , 0x210000, 0x1F0000,
```

- `otadata` (8 KiB = two 4 KiB sectors) is the boot-selection record the
  bootloader/maintenance code alternates between.
- The two app slots are 0x1F0000 = 1,984 KiB each and the table ends exactly at
  0x400000, i.e. it fills the 4 MB chip. Both offsets are 64 KiB aligned, which
  is required for app partitions (see ESP-IDF partition-table docs).
- With the current 1.18 MiB image each slot has ~772 KiB (63 %) headroom, and
  `idf.py build` enforces the 1,984 KiB limit automatically
  (`check_sizes.py` prints "Smallest app partition is 0x1f0000").
- Prefer `nvs`/`phy_init` at their current offsets: Wi-Fi credentials survive
  the migration.

Alternatives and why they were rejected:

- `factory` + `ota_0` + `ota_1` at 1.25 MiB each leaves under 70 KiB headroom
  per slot and a third copy that must also fit — too tight, and the `factory`
  image can never be updated by OTA.
- `factory` + one OTA slot gives no second target for back-to-back updates and
  no rollback.
- Using the built-in `partitions_two_ota.csv` (3 × 1 MiB) does not fit the
  1.18 MiB image at all.

The `factory` partition is not needed: with a blank/invalid `otadata` and no
factory partition the bootloader boots the first OTA slot ("No factory image,
trying OTA 0") and initializes `otadata` itself.

## 4. One-time migration (wired, once per device)

Run from the repo root with the IDF v6.0.1 environment active
(`source ~/.espressif/tools/activate_idf_v6.0.1.sh`).

```sh
# 1. edit partitions.csv as in section 3
# 2. build
idf.py build
# 3. flash bootloader (only if it changed), new partition table and app -> ota_0 (0x20000)
idf.py -p /dev/ttyACM0 flash
# 4. erase the stale otadata sector (it currently contains old factory-app bytes)
python -m esptool --chip esp32c3 -p /dev/ttyACM0 erase-region 0x10000 0x2000
```

Notes:

- `idf.py flash` chooses the app partition as IDF's "boot default"
  (`--partition-boot-default`): `factory` if present, otherwise `ota_0`. With
  the new table the initial image goes to `ota_0` automatically, and otadata
  stays blank.
- Losing NVS is not necessary. The nuclear option, if in doubt, is
  `idf.py -p … erase-flash` followed by `idf.py -p … flash` (erases Wi-Fi
  credentials and the seeded SSID config; the captive portal then re-provisions).
- **Wired reflash later**: `idf.py flash` always writes `ota_0`, but if a
  previous OTA update set `otadata` to `ota_1`, the device will keep booting the
  OTA image. After any wired flash where you want the freshly flashed image,
  erase otadata again (`erase-region 0x10000 0x2000`). NVS is untouched by that.
- Verify the first boot: the bootloader logs `Loaded app from partition at
  offset 0x20000`, and `GET /api/ota` (section 8) reports `ota_0` as the running
  slot.

## 5. sdkconfig: rollback support

Add to `sdkconfig.defaults`:

```
# Allow the bootloader to roll back an OTA image that never confirms itself.
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
```

Do not set the deprecated alias `CONFIG_APP_ROLLBACK_ENABLE` by hand. With this
option and OTA update, the new image boots in the `PENDING_VERIFY` state; the
application must call `esp_ota_mark_app_valid_cancel_rollback()` once it
considers itself healthy, otherwise the next boot (or a crash/WDT) reverts to
the previous image. While the state is still `PENDING_VERIFY`,
`esp_ota_begin()` refuses to start another update
(`ESP_ERR_OTA_ROLLBACK_INVALID_STATE`).

Recommendation for this project: confirm early — `ota_init()` (section 6) calls
it once the panel and NVS are up, before the main loop. Rationale: the existing
long-press handler does `wifi_clear_credentials(); esp_restart()`
(`main/state.cpp` / `main/device_hw.cpp`); a reset during `PENDING_VERIFY` would
silently undo the update. If stricter "must have connected to Wi-Fi once" is
wanted instead, move the call to the first `WIFI_CONNECTED` event and accept the
rollback on a device that is out of range.

Rollback is optional: without it pushes still work, but a broken image needs a
wired reflash to recover.

## 6. Firmware design

### 6.1 New module: `main/ota.h` / `main/ota.cpp`

```cpp
// main/ota.h
#pragma once

#include <esp_err.h>
#include <esp_http_server.h>

// Push OTA over the config server. Handlers run on the httpd task; ota_process()
// runs on the main task. See ota_plan.md and docs/openapi.yaml.

esp_err_t ota_init(void);                      // call once from app_main
void ota_process(void);                        // call once per main-loop tick

esp_err_t ota_handle_get(httpd_req_t *req);    // GET  /api/ota
esp_err_t ota_handle_post(httpd_req_t *req);   // POST /api/ota (octet-stream)
```

Internal state (single upload at a time; one writer = the httpd task):

```cpp
enum class OtaState { IDLE, RECEIVING, READY, ERROR };
static std::atomic<OtaState> s_state;
static std::atomic<uint32_t> s_received;
static std::atomic<uint32_t> s_total;
static std::atomic<int64_t>  s_restart_at_ms;   // 0 = no restart pending
static char s_error[64];
static uint8_t *s_buf;                          // 4 KiB, allocated in ota_init()
```

Upload flow (`ota_handle_post`):

1. Reject with `501`/`500` if `esp_ota_get_next_update_partition(nullptr)` is
   null (host build or single-app partition table). On `CONFIG_IDF_TARGET_LINUX`
   return `501 Not Implemented` — the config server runs in the simulator too.
2. If a transfer is in progress (`s_state == RECEIVING`) → `409 Conflict`.
3. If `req->content_len` is known and larger than the target partition size →
   `413 Payload Too Large` (cheap early reject; chunked uploads without a length
   are checked implicitly by `esp_ota_write`).
4. `esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &handle)` — incremental
   erase keeps each flash operation short instead of a multi-second bulk erase.
   On `ESP_ERR_OTA_ROLLBACK_INVALID_STATE` → `409` with the hint that the
   running image has not been confirmed yet.
5. `for (;;)`: `httpd_req_recv(req, s_buf, 4096)`; on `> 0`
   `esp_ota_write(handle, s_buf, n)` and update `s_received`; on `<= 0`
   (`HTTPD_SOCK_ERR_TIMEOUT` or client gone) → `esp_ota_abort(handle)`, state
   `IDLE`, return `400`. The old firmware is untouched.
6. `esp_ota_end(handle)` validates the image (structure, segment checksums,
   appended hash); truncated or garbled uploads fail here
   (`ESP_ERR_OTA_VALIDATE_FAILED`) → `400`, no boot-partition change. Note that
   verification does not tie the binary to this project — always upload a bin
   built by this repo.
7. `esp_ota_set_boot_partition(target)`; read the uploaded version with
   `esp_ota_get_partition_description(target, &desc)`; respond `202` with JSON;
   set `s_restart_at_ms = millis() + 800`.

`ota_process()` (main task, called from the `while (true)` loop):

- while a restart is pending, optionally `popup_show()` a small "update ok"
  frame (artwork belongs in `main/overlays.hpp` like the other overlays — this
  is the only place allowed to draw);
- once the deadline passes, `esp_restart()`.

`ota_init()`:

- `esp_ota_mark_app_valid_cancel_rollback()` (no-op when rollback is disabled),
- log running/boot partition + version, allocate the 4 KiB receive buffer,
- return `ESP_OK` even if rollback is not compiled in.

Rules this design keeps from [[DEVELOPER.md]]:

- The OTA handler runs on the httpd task, never on the panel/main tasks. Flash
  writes stall the instruction cache for a few ms per 4 KiB chunk; the 500 Hz
  panel loop is independent of flash and keeps running, only Wi-Fi throughput
  dips briefly.
- Nothing new is drawn from the httpd task: `ota_process()` is the main-task
  hook for the popup/restart, matching the single-writer rule.
- Buffers are heap-allocated once (4 KiB); the HTTP server stack stays at its
  current 6144 bytes.
- `esp_http_server` is single-task: `GET /api/ota` cannot be served while an
  upload is in flight. Client-side upload progress (browser/curl) is the UX;
  the server-side counter is for before/after inspection.

### 6.2 Wiring into existing files

`main/CMakeLists.txt` — add `app_update` (phase 2: `esp_https_ota`) to `reqs`.
`esp_http_server` and `cjson` are already there.

`main/config_server.cpp` — include `"ota.h"` and add two entries to the `entries[]`
table (7 handlers are used, `max_uri_handlers` is 12, so no bump needed):

```cpp
{"/api/ota", HTTP_GET,  ota_handle_get},
{"/api/ota", HTTP_POST, ota_handle_post},
```

`main/main.cpp` — one init call and one loop call:

```cpp
  ESP_ERROR_CHECK(app_control_init());
  ESP_ERROR_CHECK(ota_init());          // <-- new
  ...
  while (true) {
    app_control_process();
    ota_process();                      // <-- new
    wifi_supervisor_tick();
    StateMachine::instance().update();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
```

### 6.3 Security note

`docs/openapi.yaml` currently says "no authentication, local network only".
An OTA endpoint upgrades that stance to "remote code execution for anyone on
the LAN". Phase 1 accepts that (the device sits on a WPA2 home network). If it
should be tighter later: require a token header on `POST /api/ota`, and/or
require a button press on the device to accept an upload (physical presence),
and/or run the config server over TLS (`esp_https_server`). At minimum, never
expose the config server to the internet.

## 7. REST API shape

`GET /api/ota` — OTA readiness and running state:

```json
{
  "state": "idle",
  "running": { "label": "ota_0", "offset": 131072, "version": "1.0.0" },
  "next":    { "label": "ota_1", "offset": 2162688 },
  "received": 0,
  "total": 0,
  "last_error": null,
  "restart_pending": false
}
```

`POST /api/ota` — body = raw app image (`application/octet-stream`), e.g.
`build/obegransad.bin`. Responses:

| Status | Meaning |
|---|---|
| `202 Accepted` | image written and validated; boot slot switched, restart imminent |
| `400` | transfer aborted/timeout, or image failed validation |
| `409` | upload already in progress, or running image is not confirmed yet |
| `413` | `Content-Length` larger than the target partition |
| `500` | flash error / OTA setup failed |
| `501` | build has no OTA partitions (Linux host build, or a table without OTA slots) |

Success body (also useful to log/return as `X-Ota-Version`):

```json
{ "success": true, "version": "1.0.1", "slot": "ota_1", "restarting": true }
```

For `docs/openapi.yaml` (owned by the config-server change — coordinate before
editing), add under `tags` an `ota` tag and:

```yaml
  /api/ota:
    get:
      tags: [ota]
      summary: OTA readiness and running slot
      responses:
        "200":
          description: OTA state
          content:
            application/json:
              schema:
                $ref: "#/components/schemas/OtaStatus"
    post:
      tags: [ota]
      summary: Upload and apply a firmware image
      description: |
        Body is the raw app image (application/octet-stream), e.g.
        build/obegransad.bin. The device writes the inactive slot, verifies the
        image, switches the boot partition and restarts. No authentication:
        LAN only, see ota_plan.md §6.3.
      requestBody:
        required: true
        content:
          application/octet-stream:
            schema:
              type: string
              format: binary
      responses:
        "202":
          description: Image accepted; restart imminent
        "400":
          $ref: "#/components/responses/BadRequest"
        "409":
          description: Upload in progress or running image not confirmed
        "413":
          description: Image larger than the target partition
        "500":
          description: Flash or OTA setup error
        "501":
          description: Build has no OTA partitions
```

```yaml
    OtaStatus:
      type: object
      properties:
        state:
          type: string
          enum: [idle, receiving, ready, error]
        running:
          type: object
          properties:
            label: {type: string, example: ota_0}
            offset: {type: integer, format: int64, example: 131072}
            version: {type: string, example: "1.0.0"}
        next:
          type: object
          properties:
            label: {type: string, example: ota_1}
            offset: {type: integer, format: int64, example: 2162688}
        received: {type: integer, example: 0}
        total: {type: integer, example: 0}
        last_error: {type: string, nullable: true}
        restart_pending: {type: boolean}
```

## 8. Build and push walkthrough

```sh
# from the repo root, IDF environment active
source ~/.espressif/tools/activate_idf_v6.0.1.sh

# 1. build (also checks the image against the 1984 KiB slot)
idf.py build
ls -l build/obegransad.bin          # ~1.24 MB

# 2. find the device IP: idf.py -p /dev/ttyACM0 monitor shows "Got IP: …",
#    or use the router / the Home Assistant integration's configured host.

# 3. push the image (uploads ~1.2 MB; expect ~10–30 s on Wi-Fi)
curl -v -X POST \
     -H 'Content-Type: application/octet-stream' \
     -H 'Expect:' \
     --data-binary @build/obegransad.bin \
     http://<device-ip>/api/ota

# 4. the device validates, switches the boot slot, shows a popup and reboots
#    (watch: idf.py -p /dev/ttyACM0 monitor; verify: curl http://<device-ip>/api/ota)
```

- `-H 'Expect:'` disables curl's `Expect: 100-continue` handshake, which
  `esp_http_server` does not implement; without it the first request can stall.
- `--data-binary` is required — `-d` would mangle the binary.
- The upload is atomic from the bootloader's point of view: until step 6–7 of
  the flow succeed, `otadata` is untouched and the old image keeps booting, so
  an interrupted upload or power loss is harmless.
- Optional (config-page work, `main/config_page.h`): a small "firmware update"
  form that POSTs the file with `fetch()` and shows upload progress
  `XMLHttpRequest.upload.onprogress`; the endpoint is the same.
- No mDNS is compiled in today (`obegransad.local` in the OpenAPI is the
  documented default, not a running service) — use the IP address.

## 9. Push vs pull

| | Pull (`esp_https_ota`) | Push (this plan) |
|---|---|---|
| Initiator | device downloads a URL | browser/curl uploads to device |
| Server on device | none | config server (already being added) |
| Hosting needed | yes (HTTPS URL or release asset) | no |
| Reachability | outbound only, works behind NAT | device must be reachable on the LAN |
| Trigger | boot/button/timer/manifest poll | explicit user action |
| Auth | TLS + device-side cert/URL decision | device accepts anyone on the LAN |
| Added firmware | `esp_https_ota` + logic (~15–30 KB; `esp_http_client` already linked) | `app_update` + logic (~10–15 KB; verify with `idf.py size` after implementing) |
| Update policy | can check a version/sha manifest, no user interaction | manual each time |

Phase 2 pull (only if wanted): add `esp_https_ota` to `reqs`, read the URL from
NVS (`wifi` namespace, key `ota_url` — the captive portal's advanced page already
writes it), and call:

```c
esp_http_client_config_t http = {
    .url = ota_url,
    .crt_bundle_attach = esp_crt_bundle_attach,  // already in sdkconfig.defaults
};
esp_https_ota_config_t cfg = { .http_config = &http };
if (esp_https_ota(&cfg) == ESP_OK) {
  esp_ota_mark_app_valid_cancel_rollback();
  esp_restart();
}
```

HTTPS works out of the box (certificate bundle + HTTP client are already in the
image). Plain `http://` requires `CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=y` and is
documented by Espressif as test-only. A version manifest (JSON with
`version`/`sha256`/`url`) should gate repeat downloads.

## 10. Testing checklist

1. `idf.py build` succeeds and reports `Smallest app partition is 0x1f0000`.
2. Migration flash boots with `Loaded app from partition at offset 0x20000`;
   `GET /api/ota` shows `ota_0` running, `ota_1` next.
3. Push a valid image with a bumped version → device reboots into `ota_1`,
   `/api/ota` shows the new version and slot.
4. Push the same image again → back to `ota_0` (alternation works).
5. Push garbage (e.g. first 100 KiB of the bin) → `400`, device keeps booting
   the old slot.
6. Abort curl mid-upload (`Ctrl-C`) → old slot still boots; a new upload is
   accepted afterwards.
7. Cut power during an upload → old slot boots; `otadata` unchanged.
8. Rollback (with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`): temporarily make
   the boot path fail before `esp_ota_mark_app_valid_cancel_rollback()` (e.g.
   `abort()` right after `ota_init()`), push it, watch the bootloader revert to
   the previous slot after the failed boot.
9. Wired reflash after an OTA update: verify the `otadata` caveat in section 4.
10. Optional: run the same flow under QEMU (`tools/sim/run-qemu.sh`, flash is a
    host file); useful for CI, but real Wi-Fi timing needs the board.

## 11. Known unresolved issue: the config server and the captive portal cannot coexist

Verified still present at commit `f50ec92` ("config server + shared runtime
control layer") by reading the source, not yet observed on a live boot: the
current `sdkconfig.defaults.local` sets `CONFIG_OBG_WIFI_SSID="Vodafone-B794"`,
so the portal never starts on the dev board. It is documented here because the
OTA transport depends on the config server surviving boot.

**Symptom**: on hardware built without a configured `CONFIG_OBG_WIFI_SSID` — or
whenever the captive portal starts while the config server is already running —
the device aborts with an `ESP_ERROR_CHECK` failure in the HTTP server setup.

**Chain**:

1. `main/main.cpp:53` — `wifi_init()`. When `CONFIG_OBG_WIFI_SSID` is empty (the
   tracked default in `sdkconfig.defaults`) it calls `start_captive_portal()`
   (`main/device_hw.cpp:167`), which runs
   `WifiConfigurationAp::Start()` →
   `StartWebServer()` (`managed_components/78__esp-wifi-connect/wifi_configuration_ap.cc:224-233`)
   → `httpd_start()` with `HTTPD_DEFAULT_CONFIG()`: server port 80, control
   port 32768.
2. `main/main.cpp:92` — `ESP_ERROR_CHECK(config_server_start())`. The config
   server (`main/config_server.cpp`) also uses `HTTPD_DEFAULT_CONFIG()`, only
   overriding `config.server_port = CONFIG_OBG_HTTP_PORT` (80 on device) —
   `ctrl_port` stays at the default 32768.
3. Every `esp_http_server` instance gets a UDP control socket bound to
   `127.0.0.1:<ctrl_port>`
   (`components/esp_http_server/src/util/ctrl_sock.c`), without
   `SO_REUSEADDR`. lwIP's `udp_bind()` rejects a second bind of the same
   address/port with `ERR_USE` unless both sockets have `SO_REUSEADDR`
   (`components/lwip/lwip/src/core/udp.c`). The second `httpd_start()` therefore
   fails ("error in creating ctrl socket") and returns `ESP_FAIL`.
4. Whichever server starts second aborts via its `ESP_ERROR_CHECK`. At boot with
   an empty SSID that is `config_server_start()` at `main.cpp:92` (the portal
   won at line 53); if the portal is started later instead — the `SETUP` state
   path, `main/state.cpp:266`, e.g. after credentials are cleared while the
   device is running — then the component's own
   `ESP_ERROR_CHECK(httpd_start(...))` is the one that aborts.

**Secondary issue** even after the control ports are separated: both servers
listen on TCP port 80. `esp_http_server` sets `SO_REUSEADDR` on the listening
socket and lwIP allows both binds, but each connection is delivered to only one
listener, so one of the two UIs silently becomes unreachable.

**Why the simulator does not catch it**: the QEMU and native host builds replace
`start_captive_portal()` with a stub (`main/device_sim.cpp:137`,
`main/device_host.cpp:79`), so no second `httpd` is created there. The bug is
hardware-only, and invisible with the dev board's local SSID.

**Resolution options** (pick one, then this section can go):

- give the two servers distinct control *and* server ports (e.g.
  `config.ctrl_port = ESP_HTTPD_DEF_CTRL_PORT + 1` and
  `config.server_port = 8080` in `config_server.cpp`, and the same treatment in
  the portal's `StartWebServer()`); or
- keep only one web UI: let the config server own port 80 and drop the portal's
  web server (its page overlaps with the config UI); or
- serialize the lifetimes: start the config server only once STA has an IP
  (`wifi_check()` / `WIFI_CONNECTED`) and `httpd_stop()` it before
  `start_captive_portal()` runs.

Whichever option is chosen, `/api/ota` should only be reachable in STA mode.

**How to reproduce**: build with `CONFIG_OBG_WIFI_SSID=""` (or clear the
credentials first), flash the hardware build, and watch the monitor at boot —
expect `httpd_start failed: ESP_FAIL` / `config_server_start() returned: ESP_FAIL`
before the main loop starts.

## 12. Coordination / known gotchas

- The config-server/captive-portal conflict is tracked separately in section 11.
- `GET /api/ota` cannot report progress during an upload (single httpd task) —
  see section 6.1.
- `esp_ota_begin` refuses to run while the image is `PENDING_VERIFY`; that is
  why `ota_init()` confirms the app (section 5).
- Host build (`IDF_TARGET_LINUX`) has no partitions → handlers must return
  `501`; the simulator/QEMU esp32c3 build can work against its flash file.
- Keep the receive buffer off the httpd stack (`stack_size = 6144` in
  `config_server.cpp`); 4 KiB on the heap is enough, no config bump needed.
- Image verification checks that the upload is a structurally valid ESP32 image;
  it does not tie it to this project, so only ever upload a bin produced by this
  repo's `idf.py build`. Flash every UART-recovered device with the new
  partition table before expecting OTA to work.

## 13. References

- ESP-IDF OTA overview and partition requirements:
  `$IDF_PATH/docs/en/api-reference/system/ota.rst`
  (local: `~/.espressif/v6.0.1/esp-idf/docs/...`)
- `esp_https_ota` API and Kconfig:
  `components/esp_https_ota/include/esp_https_ota.h`,
  `components/esp_https_ota/Kconfig` (`ESP_HTTPS_OTA_ALLOW_HTTP`)
- Partition tables, alignment rules, `partitions_two_ota.csv` example:
  `components/partition_table/`, `docs/en/api-guides/partition-tables.rst`
- Low-level OTA API: `components/app_update/include/esp_ota_ops.h`
- Rollback Kconfig: `components/bootloader/Kconfig.app_rollback`
- IDF examples: `examples/system/ota/simple_ota_example` (push-style),
  `examples/system/ota/native_ota_example` (pull), `advanced_https_ota`
- Project: `partitions.csv`, `sdkconfig.defaults*`, `main/config_server.cpp`,
  `main/main.cpp`, `docs/openapi.yaml`, and the portal component's delayed
  restart pattern in
  `managed_components/78__esp-wifi-connect/wifi_configuration_ap.cc` (`/reboot`)
- Image sizes in this plan come from `build/` (September 2026) and
  `idf.py size-components`.
