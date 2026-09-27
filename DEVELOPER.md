# Project Description (event-driven)

## Goal

Turn the 16×16 grayscale matrix into a resilient smart display with:

* deterministic 400–500 Hz refresh,
* Wi-Fi credentials from Kconfig (the captive portal was removed, see
  docs/known-issues.md),
* an embedded HTTP configuration server,
* weather fetching when connected, and
* a single physical button with context-dependent actions.

The system is implemented as cooperating tasks and an event bus rather than a single global loop state machine.

---

# High-level architecture (ASCII)

```
                    +-----------------------------+
                    |   esp-idf / lwIP / Wi-Fi    |
                    +-------------+---------------+
                                  |
       +--------------------------+-------------------------+
       |                          |                         |
+------v------+         +---------v---------+      +--------v--------+
| HTTP Server |         | Weather Client    |      | Wi-Fi Manager   |
| (low prio)  |         | (periodic task)   |      | (esp_event posts)|
+-------------+         +-------------------+      +-----------------+
                                  |
                                  v
                       +-------------------------+
                       |   Application / UI      |
                       |   (state machine task)  |
                       +-----------+-------------+
                                   |
                    event queue / esp_event posts
                                   |
             +---------------------+---------------------+
             |                                           |
+------------v------------+                 +------------v------------+
| Display Task (HIGH)     |                 | Button Handler Task     |
| - owns framebuffer      |                 | - ISR -> queue -> task  |
| - triggered by esp_timer|                 | - debouncing, events    |
+------------+------------+                 +-------------------------+
             ^
             |
     esp_timer ISR  (IRAM_ATTR)  -> vTaskNotifyGiveFromISR(panel_task)
```

---

# Module responsibilities

### Display (highest-determinism)

* Owns the framebuffer and the "commit" pipeline.
* Receives a **task notification** from an `esp_timer` ISR at each refresh tick (use `ESP_TIMER_ISR` dispatch; ISR
  should only notify).
* Runs in a dedicated FreeRTOS task (non-ISR) so it can call blocking SPI/RMT calls safely.
* Must be short and deterministic. No heap allocations, no filesystem, no network.
* Overlay rendering (status icons) is composed in memory and blended just before `commit`.

**Key API (example)**:

```cpp
// ISR (IRAM_ATTR)
static void IRAM_ATTR refresh_timer_callback(void *arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(g_panel_task_handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Display task
void panel_task(void* _) {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // wait for tick
    refresh_display_frame();                 // deterministic, returns quickly
  }
}
```

(You already had this pattern — keep it.)

---

### Button handling

* GPIO ISR (IRAM) must be minimal: push an event into an ISR-safe queue (or set an event group bit).
* A *Button Handler Task* reads the queue, debounces, recognizes short/long/double presses and posts higher-level events
  to the app event bus.

**As implemented:** `button.cpp` polls (10 ms) rather than using a GPIO ISR — simpler, and cheap enough at this poll
rate — but still isolates all debounce/classification logic in its own task, posting exactly the three events the
spec calls for. In `OPERATIONAL`/`DEGRADED`, `StateMachine` maps them to: short press → `preset_next()`, double press
→ `preset_prev()` (both followed by `show_preset_popup()`, see "Scene presets" and "Rendering loop" below). Long
press is a no-op (it used to be the factory reset; see docs/known-issues.md); the event is still posted so external
clients can observe it.

### Wi-Fi Manager

* Uses `esp_event` to announce `WIFI_CONNECTED`, `WIFI_DISCONNECTED`.
* Credentials come from Kconfig (`CONFIG_OBG_WIFI_SSID`/`_PASSWORD`, normally
  in the gitignored `sdkconfig.defaults.local`). The captive portal was removed
  (see docs/known-issues.md): with no credentials the device stays offline
  until it is reflashed.
* The config server (port 8080) is started by the app itself; see
  docs/openapi.yaml.
* Recover from disconnections automatically (exponential backoff).

**As implemented:** the vendored `78/esp-wifi-connect` does the reconnecting (5 immediate attempts, then the next AP
from its scan queue), but it surfaces every failed attempt as another `WIFI_EVENT_STA_DISCONNECTED`. `device.cpp`
therefore runs a small supervisor — `wifi_supervisor_tick()`, called from the main loop, no extra task or timer:

* a raw disconnect only starts a timer; `APP_EVT_WIFI_DISCONNECTED` is posted once the link has stayed down for
  `CONFIG_OBG_WIFI_DOWN_GRACE_MS`, which stops the display flapping OPERATIONAL↔DEGRADED during normal reconnects,
* `APP_EVT_WIFI_CONNECTED` is posted only on a real up-edge, so the two events always pair,
* while down, `esp_wifi_disconnect()` + `esp_wifi_connect()` are forced on a 30 s → 300 s backoff in case the
  component's own retries gave up.

Not fixed, because it lives in `managed_components/`: `wifi_station.cc` re-arms its rescan timer with `10 * 1000`
microseconds (10 ms, almost certainly meant to be 10 s), so a long outage keeps that component scanning in a tight
loop.

### HTTP Server (config UI)

* Runs in its own task(s) (esp_http_server).
* Low priority; must not block display operations.
* Request handlers should be fast — long operations schedule work on the HTTP client / worker task.

### Weather client

* Periodic task or a worker spawned on demand.
* Uses event-driven timers (FreeRTOS software timer or esp_event + delayed post).
* Heavy network activity runs at normal task priority — do not run in display task.

### Scenes

* Pure rendering logic: `activate()`, `deactivate()`, `update(dt)`, `requires_wifi()`.
* Called by the Application/UI task. Scenes only draw pixels — they do not manage peripheral state or network.

### Config / NVS

* Central small API for get/set config values.
* The Wi-Fi manager consumes config for connectivity.

### Clock / NTP

* Runs when Wi-Fi is available; posts `TIME_SYNCED` event to the event bus.

---

# Event-driven state model (replace the global state loop)

Use `esp_event` (ESP-IDF event loop) or a small internal event bus. Post named events instead of spinning in a loop.

**Core events (examples)**:

* `EVT_BOOT`
* `EVT_WAKE`
* `EVT_GO_TO_SLEEP`
* `EVT_WIFI_CONNECTED`
* `EVT_WIFI_DISCONNECTED`
* `EVT_CAPTIVE_PORTAL_ACTIVE`
* `EVT_BUTTON_SHORT`
* `EVT_BUTTON_LONG`
* `EVT_BUTTON_DOUBLE`
* `EVT_WEATHER_DATA_READY`
* `EVT_ERROR_OCCURED`

**State examples** (application-level):

* `SLEEPING`
* `SETUP` was removed together with the captive portal; the states are `OPERATIONAL`, `DEGRADED`, `ERROR`, `SLEEPING`.
* `OPERATIONAL`
* `DEGRADED`
* `ERROR`

**Transition principle**: The *Application Task* subscribes to events and updates a local state enum. State transitions
can trigger actions (start/stop services) by posting control events to modules (e.g., `START_HTTP_SERVER`,
`STOP_SCENE_SWITCHER`).

---

# Timing, jitter and Wi-Fi CPU budget

* Keep display refresh short. Use DMA / non-blocking SPI where available (or short, deterministic polling SPI). Measure
  the worst-case refresh duration.
* Ensure display duty per tick << tick period. For 400 Hz, period = 2.5 ms. Aim for display work < ~400–600 µs to
  preserve headroom for Wi-Fi.
* Use `ESP_TIMER_ISR` + `vTaskNotifyGiveFromISR` for the most deterministic tick timing; the actual heavy work (SPI /
  RMT) must be done in the display task (task context).
* Do **not** call logging or `malloc` inside the display refresh path — these can block and cause Wi-Fi starvation.
* If refresh ever needs more CPU than acceptable, move more to DMA or reduce bit-depth or split the refresh across
  multiple ticks.

---

# Priorities & resource guidance (relative scheme)

Use a relative priority scheme rather than absolute numbers when possible. Example ordering (higher == more important):

1. `Display Task` — **Highest** (must preempt others briefly on refresh)
2. `Wi-Fi / lwIP internal tasks` — **Very High** (ensures network runs correctly)
3. `HTTP Server / Weather client` — **High**
4. `Application / UI Task` — **Normal**
5. `Button Handler / Background` — **Low**
6. `Idle/Background` — **Lowest**

If your `configMAX_PRIORITIES` is small, place Display at the top (max - 1), Wi-Fi slightly below it, etc. Tune by
measuring `uxTaskGetSystemState()` and CPU usage.

**Stack sizes**: measure, but typical starting points:

* Display task: 4096–8192 bytes (depends on local buffers).
* HTTP server tasks: 6–12 KB (esp_http_server can be heavier).
* UI task: 4096 bytes.
* Button task: 2048 bytes.

Adjust after profiling.

**As implemented** — every FreeRTOS task that exists once the app is running, with its actual priority
(`configMAX_PRIORITIES` is 25 on this target, so the valid range is 0–24; ESP-IDF's own scheme is documented in
`esp_task.h` and `docs/api-guides/performance/speed.rst`):

| Task | Priority | Created by | Touches the panel? |
|---|---|---|---|
| `panel_task` | **24** (`configMAX_PRIORITIES - 1`) | `panel_init()` (`ikea-obegransad-panel.c`) | Yes — the only *reader* of `g_framebuffer`. Woken every 2 ms by the refresh-timer ISR; see "Rendering loop" above. Espressif's guidance caps non-networking tasks at 24 for exactly this "very short, restricted" use, so this is the system max, used correctly. |
| Wi-Fi driver task | 23 (fixed by the Wi-Fi library, not app-configurable) | `esp_wifi_start()` inside `WifiStation::Start()` | No |
| `esp_timer` service task | 22 (`ESP_TASK_TIMER_PRIO`) | ESP-IDF startup | No — runs any `ESP_TIMER_TASK`-dispatched callback (the managed component's `WifiScanTimer`, and the unused `RenderTimer` helper in `helper.hpp`). The panel refresh timer deliberately uses `ESP_TIMER_ISR` dispatch instead, so it runs in the ISR and never touches this task at all. |
| `esp_event` default-loop task | 20 (`ESP_TASK_EVENT_PRIO`) | `esp_event_loop_create_default()` in `device_init()` | No — runs every registered `esp_event` handler (`wifi_event_handler` in `device.cpp`, `StateMachine::on_app_event`, `status_led`'s handler). All three only record state or enqueue; none draw. |
| lwIP TCP/IP task | 18 (`ESP_TASK_TCPIP_PRIO`) | ESP-IDF network init | No — owns the network stack; `weather.cpp`'s HTTP client and SNTP do their I/O through it. |
| `weather` | 5 (explicit in `weather_client_init()`) | `weather_client.cpp` | No — blocks on `ulTaskNotifyTake` (up to 20 min), fetches, posts `APP_EVT_WEATHER_DATA_READY`. |
| `button` | 5 (explicit in `button_init()`) | `button.cpp` | No — 10 ms poll loop, posts `APP_EVT_BUTTON_*`. |
| `main` (`app_main`) | **1** (`ESP_TASK_MAIN_PRIO`, the FreeRTOS floor) | ESP-IDF startup | **Yes — the only writer.** Runs `wifi_supervisor_tick()` and `StateMachine::update()`, i.e. every scene, popup, and overlay draw in the app. See "Concurrency & synchronization" below. |
| FreeRTOS Timer Service task | 1 (`CONFIG_FREERTOS_TIMER_TASK_PRIORITY`) | FreeRTOS init | No — present but idle; nothing in this project creates a plain `xTimerCreate` software timer (everything uses `esp_timer` instead). |

The main task sitting at priority 1 — the lowest possible — is deliberate on ESP-IDF's part, so app code never
starves Wi-Fi/lwIP/the event loop above it. The trade-off is the one detailed just below: at priority 24,
`panel_task` can, and regularly does, preempt the main task mid-frame.

---

# Practical rules / coding guidelines

* **ISRs** do the absolute minimum:

    * `esp_timer` ISR: notify display task.
    * GPIO ISR: push event to queue.
* **Callbacks** on timer should be in IRAM and marked `IRAM_ATTR` if they run in ISR context.
* **Display task** may call `spi_device_polling_transmit` if deterministic, or use queued DMA (
  `spi_device_queue_transmit`) + `spi_device_get_trans_result`.
* **Avoid** `vTaskDelay()` in display callbacks.
* **Use event posting** for cross-module communication (esp_event).
* **Scene design**: `update()` receives a timestamp or delta; scenes must be pure drawing functions (no side effects).

---

# Wi-Fi credentials & config flow

The captive portal was removed (see docs/known-issues.md). Credentials are set
via `CONFIG_OBG_WIFI_SSID`/`_PASSWORD` in the gitignored
`sdkconfig.defaults.local` and take effect on the next flash; there is no
in-field provisioning path right now. Runtime settings live on the config
server (port 8080, docs/openapi.yaml).

1. Boot -> Wi-Fi Manager reads the configured SSID and connects. With no SSID
   the device logs an error and stays in `DEGRADED` until reflashed (there is
   no provisioning mode, see docs/known-issues.md).
2. On successful connection -> `EVT_WIFI_CONNECTED` -> NTP sync, weather fetch,
   scene switcher.
3. On Wi-Fi loss -> `EVT_WIFI_DISCONNECTED` -> move to `DEGRADED`: the scene
   switcher may still run, but cloud-only scenes are filtered out.
4. The config server is up in every state; `POST /api/ota` additionally
   requires a station connection.

---

# Overlay rendering & scene compositing

* Scenes draw to a shared framebuffer (or into an offscreen buffer).
* After `scene.update()` completes, the Display Task composes overlays (status icons, battery, debug dot) then commits
  the frame.
* Overlays are small and fast; implement as a final write to the framebuffer immediately before the SPI/RMT commit.

**As implemented:**

* `main/frame.hpp` — `Frame`, a plain 16×16 off-screen screenbuffer (256 bytes, no allocation). This is the type
  modules use to hand whole frames to each other; `Frame::present()` pushes it to the panel through `panel_setPixel()`
  so it renders identically to direct drawing. `drawSprite()` and the sprite/font classes in `main/sprites.hpp` take an
  optional `Frame *target` and an integer `scale`, so existing art can be composed off-screen and enlarged.
* `main/popup.{h,cpp}` — a timed fullscreen overlay: `popup_show(frame, duration_ms)`, `popup_is_active()`,
  `popup_render()`, `popup_dismiss()`. It knows nothing about scenes, presets or Wi-Fi; `StateMachine::update()` decides
  it wins by skipping scene rendering while one is active, and calls `scene_force_redraw()` on the trailing edge (see
  "Rendering loop" below for the exact preemption order). Duration for the app's own popups is
  `CONFIG_OBG_POPUP_MS` (default 2000 ms).
* `main/overlays.hpp` — the app's popup artwork (`overlay_preset_number()`, `overlay_wifi()`), kept out of the popup
  module so that stays generic.

---

# Scene presets

Instead of one global rotation over every registered scene, `main/presets.hpp` defines up to ten numbered presets
(0–9), each an ordered list of scenes plus its own dwell time (`dwell_ms == 0` means "never rotate"). The order in the
list *is* the rotation, so alternating e.g. clock and weather is just a matter of writing them alternately; empty slots
are skipped when cycling.

Scenes are referenced by their `get_scene_name()` string and resolved once to registry indices in
`scene_switcher_init()`; unresolvable names are logged and dropped. A short button press selects the next preset, a
double press the previous one, and both confirm with a fullscreen number popup. If a preset has no displayable scene
right now (a weather-only preset while Wi-Fi is down) the switcher falls back to the first scene that does not
`requires_wifi()` and returns to the preset's rotation once Wi-Fi is back.

---

# Rendering loop (as implemented)

Two independent, differently-clocked loops cooperate through one shared resource: the panel driver's framebuffer
(`g_framebuffer`, private to `components/ikea-obegransad-panel/ikea-obegransad-panel.c`).

### 1. Hardware refresh loop — 500 Hz, `panel_task`

* An `esp_timer` fires every `FRAME_PERIOD_US` (2000 µs) with `dispatch_method = ESP_TIMER_ISR`; the ISR
  (`refresh_timer_callback`, `IRAM_ATTR`) does nothing but `vTaskNotifyGiveFromISR(g_panel_task_handle, ...)`.
* `panel_task_func` (priority `configMAX_PRIORITIES - 1`, pinned to the last core) wakes on that notification. If
  `g_refresh_needed` is set (i.e. something called `panel_commit()` since the last tick) it rebuilds all four bit
  planes from `g_framebuffer` in one go (`prepare_bitplane()` × `BIT_DEPTH`); either way it then shifts out the
  current plane over SPI and pulses `OE` via RMT for that plane's on-time (50/100/200/400 µs for planes 0–3), then
  advances `g_plane_idx` for the next tick.
* Effect: a full 4-plane BCM cycle takes 4 ticks = 8 ms (~125 Hz perceived full-brightness refresh), and any pixel
  write becomes visible within at most one 8 ms cycle of the next `panel_commit()`. This loop never blocks on
  anything outside the panel component and has no notion of scenes, popups, or app state — it only ever reads
  whatever `g_framebuffer` currently holds.

### 2. Application loop — 100 ms, `app_main()`

```cpp
while (true) {
  wifi_supervisor_tick();
  StateMachine::instance().update();
  vTaskDelay(pdMS_TO_TICKS(100));
}
```

This is the *only* place that writes into `g_framebuffer` (via `panel_setPixel`/`panel_commit`, whether directly,
through a scene, or through a `Frame`), so all rendering is single-threaded with respect to itself — the 500 Hz
loop only ever reads a framebuffer this loop finished writing to. The 100 ms period is also a hard ceiling on scene
animation rate: `Scene::update()` throttles to `target_fps()`, but no scene can exceed 10 fps in practice because
nothing calls `tick()` more often than every 100 ms.

`StateMachine::update()` decides **what** gets drawn on a given tick, in strict priority order. Each level below
*preemptively takes over* from the next — lower levels are not evaluated at all while a higher one is active,
which is different from compositing (see the DEGRADED dot at the bottom, which *is* a composite):

1. **Popup** (`popup_is_active()`) — highest priority. `process_events()` always runs first, so button and Wi-Fi
   events keep being queued even during a popup; but if the popup is active, `update()` calls `popup_render()`
   (which just re-`present()`s the stored `Frame`) and returns immediately — no scene ticks, and the DEGRADED dot
   is not drawn. On the trailing edge (the first tick where `popup_is_active()` has gone false),
   `StateMachine` calls `scene_force_redraw()`, which invokes `Scene::request_redraw()` on the active scene. That
   resets the FPS-throttle's `has_rendered` flag *and* calls the scene's `on_redraw_requested()` hook, then
   `reset_scene_dwell()` gives the scene a full dwell interval. The hook matters for scenes with their own
   change-detection cache — `ClockScene`/`ClockSceneWithSecondHand` only redraw when the minute/second changes —
   without it, such a scene could stay frozen on the popup's leftover pixels for up to a minute.
2. **`AppState`** — exactly one of `OPERATIONAL`, `DEGRADED`, `ERROR`, `SLEEPING` is active at a time, each
   with its own branch in `update()`'s `switch`. `ERROR`
   (`panel_clear(); font_bold.drawGlyph('!', 4, 4);`) fully replace scene rendering rather than compositing with
   it — a takeover, not an overlay.
3. **Scene rotation** — only reached from `OPERATIONAL`/`DEGRADED`. Before drawing, `update()` checks whether the
   active preset's dwell has elapsed (`preset_dwell_ms()`; 0 means the preset never rotates) and, if so, calls
   `rotation_advance()`. Then `tick()` renders the current scene: `Scene::update()` → FPS throttle → `render(dt)`
   → the scene's own `panel_clear()`/draw calls/`panel_commit()`.
4. **DEGRADED status dot** — the one genuine *overlay* in the pipeline, not a takeover: after `tick()` runs inside
   the `DEGRADED` branch, `panel_setPixel(15, 0, PANEL_BRIGHTNESS_1); panel_commit();` draws a single pixel
   (bottom-left) on top of whatever the scene just committed. It is redrawn every DEGRADED tick regardless of
   whether the scene itself redrew that tick, which is what lets it survive scenes that skip frames under their
   own FPS throttle.

### Two framebuffers, one pipeline

`g_framebuffer` (panel-private, `uint8_t[16][16]`) and `Frame` (`main/frame.hpp`, the app-level exchangeable
screenbuffer introduced for popups) are deliberately different types that funnel through the same API:
`Frame::present()` calls `panel_setPixel()` once per pixel and then `panel_commit()`, so a popup's frame reaches the
LEDs by exactly the same path a scene's direct drawing does — there is no special-cased "popup mode" in the panel
driver, and no double-buffering: whichever of the two writers ran most recently on the 100 ms loop is what the
500 Hz loop displays.

---

# Concurrency & synchronization (as implemented)

### Single-writer model for the panel

Every function that touches the panel — `panel_setPixel`/`panel_fill`/`panel_clear`/`panel_commit` directly, or
`Frame::present()` — is only ever called from the **main task**. This is true by construction, not convention:
`scene_switcher.h`, `popup.h` and `state.h` are only `#include`d by `main.cpp`, `state.cpp`, `scene_switcher.cpp`
and `popup.cpp`, and none of those run on another task. The other three application tasks (`button`, `weather`,
`panel_task`) never call a `panel_*` function; they only post `esp_event`s or (for `panel_task`) read the
framebuffer. So there is exactly one writer, drawing synchronously, one call at a time — the classic "many
writers" hazard doesn't exist today.

### How the popup gets exclusivity — no lock, one dispatcher

There is no mutex, semaphore, or priority mechanism giving the popup "exclusive drawing rights". It works because
`StateMachine::update()` (main task; walked through step-by-step in "Rendering loop" above) is the **only place**
that decides what gets rendered on a given 100 ms tick, and it is a plain sequential `if`/`switch`: at most one of
{`popup_render()`, the active `AppState` branch (which includes `tick()` for scene rendering)} runs per call. A
scene can never "override" a popup, because nothing ever calls `tick()` while `popup_is_active()` is true — the
scene switcher has no idea a popup exists, and doesn't need to. If a future module (an HTTP config UI, a second
data source, …) ever called `panel_setPixel`/`Frame::present()` from **its own** task, this guarantee would break
immediately: nothing in `components/ikea-obegransad-panel` stops it racing with the main task. Rule of thumb: **any
code that draws must run on the main task**, i.e. be reached through `StateMachine::update()`.

### A real (if narrow) race: torn frames between the main task and `panel_task`

`g_framebuffer` also has no lock around it, but unlike the single-writer guarantee above, this *is* a genuine
two-task race, because it is read by a task other than the one that writes it:

* `panel_setPixel()` sets `g_refresh_needed = true` on **every call**, not just when the caller finally calls
  `panel_commit()`. A scene that draws, say, 20 pixels one at a time gives 20 opportunities for
  `g_refresh_needed` to already be `true` mid-frame.
* `panel_task` (priority 24) preempts the main task (priority 1) the instant the refresh-timer ISR notifies it —
  every 2 ms, unconditionally, regardless of what the main task is doing. On this single-core target, that
  preemption can land in the middle of a scene's draw sequence.
* If it does, `prepare_bitplane()` rebuilds all four bit planes from a **partially updated** `g_framebuffer` — a
  torn frame — and displays it.
* In practice the window is small (drawing a full 16×16 frame is a few hundred trivial array writes, well under
  2 ms) and the very next `panel_setPixel` call re-arms `g_refresh_needed`, so any tearing self-corrects within
  one more 2 ms tick — at worst a single-frame flicker, not a persistent glitch. It has not been observed as
  visible in practice, but it is a real gap, not a theoretical one.
* Mitigation, if it's ever worth the complexity: wrap a frame's writes in `taskENTER_CRITICAL`/`taskEXIT_CRITICAL`
  around `g_framebuffer` (cheap, since draws are fast, but adds jitter right on the 500 Hz path — exactly what
  "Timing, jitter and Wi-Fi CPU budget" above warns against), or give the panel a second buffer and swap a single
  pointer in `panel_commit()` instead of mutating the live one in place. Neither is implemented; this is flagged
  as a known limitation rather than fixed speculatively.

### Cross-task communication stays event/queue-based

Everything that *isn't* the panel already follows the pattern the spec asks for: drivers (`button`, Wi-Fi, SNTP,
weather) only ever communicate upward by posting `esp_event`s (see "Event-driven state model" above), and
`StateMachine::on_app_event` immediately re-marshals those onto a queue so `process_events()` runs the actual
reactions back on the main task — no other task ever calls into `StateMachine`, `scene_switcher`, or `popup`
directly. The panel is the only place a genuine (if narrow) cross-task hazard exists.

---

# Debugging, testing & telemetry checklist

* Run the QEMU simulator for scene/state/network work without hardware:
  `tools/sim/run-qemu.sh` (see `tools/sim/README.md`). It replaces the panel
  output with a host window and Wi-Fi with QEMU's emulated Ethernet; SPI/RMT
  timing and real Wi-Fi still need the board.
* Measure refresh jitter (timestamp before ISR and at commit).
* Measure worst-case refresh duration and CPU utilization.
* Ensure Wi-Fi tasks get >=20% CPU during heavy network operations (simulate loads).
* Test button debouncing and long/short/double detection under load.
* Validate boot with no credentials (device offline in DEGRADED).
* Use heap and stack monitoring (heap_caps_get_free_size, `uxTaskGetStackHighWaterMark`).

---

# Migration plan (practical step-by-step)

1. **Create an event bus** (use `esp_event`).
2. **Extract Display Task** (if not already): make it a dedicated task that waits on task notifications.
3. **Change esp_timer to ISR dispatch**: set `dispatch_method = ESP_TIMER_ISR` and in the ISR notify display task with
   `vTaskNotifyGiveFromISR`.

* If you must block in the refresh (e.g., polling SPI), do it in the display task, **not** in ISR.

4. **Replace global loop** with an `app_task` that subscribes to `esp_event` events and posts commands to modules (
   start/stop server, switch scene, etc).
5. **Implement button ISR -> queue -> button task** and publish `EVT_BUTTON_*` events.
6. **Implement Wi-Fi manager** (auto reconnect) that posts events.
7. **Refactor Scenes** to be pure drawing functions and ensure `requires_wifi()` is honored by the
   app_task/scene_switcher.
8. **Profile** and tune priorities/stack sizes.
9. **Edge cases**: implement fallback degraded mode (the old global long-press
   factory reset was removed with the captive portal; see docs/known-issues.md).

---

# Short example: esp_timer args (suggested)

```cpp
esp_timer_create_args_t timer_args = {
    .callback = refresh_timer_callback,
    .name = "panel_refresh",
    .dispatch_method = ESP_TIMER_ISR, // call in ISR context (very low jitter)
};
esp_timer_handle_t refresh_timer;
esp_timer_create(&timer_args, &refresh_timer);
esp_timer_start_periodic(refresh_timer, 2500); // microseconds for 400Hz
```

(Then ISR only notifies; display task does the work.)

---

# Final notes & tradeoffs

* Your existing design pieces (framebuffer, RMT, SPI, scenes) are well structured — the biggest improvement is moving
  from a monolithic loop to an event-driven, task-based design.
* The key constraints to preserve: *no heavy work in ISRs* and *display refresh must be deterministic and short.* Use
  the ISR→notify pattern you already implemented.
* Tune the display task to be as short as possible (use DMA, partial refresh, or reduce depth if needed) so Wi-Fi has
  its required CPU budget.
