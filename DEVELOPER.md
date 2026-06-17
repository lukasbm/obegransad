# Project Description (event-driven)

## Goal

Turn the 16×16 grayscale matrix into a resilient smart display with:

* deterministic 400–500 Hz refresh,
* Wi-Fi provisioning via captive portal,
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

### Wi-Fi Manager & Captive Portal

* Uses `esp_event` to announce `WIFI_CONNECTED`, `WIFI_DISCONNECTED`, `CAPTIVE_PORTAL_STARTED`,
  `CAPTIVE_PORTAL_STOPPED`.
* Runs the AP + captive portal when there is no valid configuration.
* Starts the HTTP server once connected (or when captive portal mode requires it).
* Recover from disconnections automatically (exponential backoff).

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
* `SETUP` (captive portal)
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

# Captive portal + Web UI flow (suggested)

1. Boot -> Wi-Fi Manager loads NVS config. If no SSID => start AP + captive portal -> post `EVT_CAPTIVE_PORTAL_ACTIVE`.
2. Captive portal starts HTTP server. User configures Wi-Fi.
3. On successful connection -> `EVT_WIFI_CONNECTED` -> NTP sync, start HTTP server for device config, start scene
   switcher.
4. On Wi-Fi loss -> `EVT_WIFI_DISCONNECTED` -> move to `DEGRADED`: scene switcher may still run, but cloud-only scenes
   filtered out.

---

# Overlay rendering & scene compositing

* Scenes draw to a shared framebuffer (or into an offscreen buffer).
* After `scene.update()` completes, the Display Task composes overlays (status icons, battery, debug dot) then commits
  the frame.
* Overlays are small and fast; implement as a final write to the framebuffer immediately before the SPI/RMT commit.

---

# Debugging, testing & telemetry checklist

* Measure refresh jitter (timestamp before ISR and at commit).
* Measure worst-case refresh duration and CPU utilization.
* Ensure Wi-Fi tasks get >=20% CPU during heavy network operations (simulate loads).
* Test button debouncing and long/short/double detection under load.
* Validate captive portal flow with network off at boot.
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
6. **Implement Wi-Fi manager** (AP + captive portal + auto reconnect) that posts events.
7. **Refactor Scenes** to be pure drawing functions and ensure `requires_wifi()` is honored by the
   app_task/scene_switcher.
8. **Profile** and tune priorities/stack sizes.
9. **Edge cases**: implement fallback degraded mode, and a global long-press handler (ISR -> event) that always triggers
   reset behavior.

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
