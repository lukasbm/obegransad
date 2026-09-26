// TCP link to the host-side simulator renderer.
//
// The renderer is the server (tools/sim/renderer.py), the firmware connects as
// a client. Under QEMU the renderer runs on the host and is reachable at
// 10.0.2.2 (slirp's host alias); on a host build it is 127.0.0.1.
//
// Outbound-only means no QEMU port forwarding is needed for frames/buttons.
// The connection is retried in the background, so the renderer may be started
// at any time.

#include "obegransad-sim.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <errno.h>
#include <string.h>

#if CONFIG_IDF_TARGET_LINUX
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include "lwip/inet.h"
#include "lwip/sockets.h"
#endif

static const char *TAG = "sim_link";

static const uint8_t FRAME_MAGIC[] = {'O', 'B', 'G', 0x01};
#define SEND_INTERVAL_MS 50 // ~20 fps to the renderer
#define RECONNECT_MIN_MS 500
#define RECONNECT_MAX_MS 5000

static TaskHandle_t s_task;
static QueueHandle_t s_commands;

static void link_task(void *arg);

// Latest-frame mailbox. s_frame_lock guards s_frame and s_seq; the link task
// compares s_seq with what it last sent, so a publish during a send is never
// lost.
static uint8_t s_frame[SIM_LINK_FRAME_BYTES];
static volatile uint32_t s_seq;
static portMUX_TYPE s_frame_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t sim_link_init(void) {
  if (s_task != NULL) {
    return ESP_OK;
  }
  s_commands = xQueueCreate(8, sizeof(uint8_t));
  if (s_commands == NULL) {
    return ESP_ERR_NO_MEM;
  }
  if (xTaskCreate(link_task, "sim_link", 3072, NULL, 2, &s_task) != pdPASS) {
    vQueueDelete(s_commands);
    s_commands = NULL;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

void sim_link_publish_frame(const uint8_t *frame) {
  if (frame == NULL) {
    return;
  }
  taskENTER_CRITICAL(&s_frame_lock);
  memcpy(s_frame, frame, SIM_LINK_FRAME_BYTES);
  s_seq++;
  taskEXIT_CRITICAL(&s_frame_lock);
}

int sim_link_poll_command(void) {
  if (s_commands == NULL) {
    return 0;
  }
  uint8_t cmd = 0;
  return xQueueReceive(s_commands, &cmd, 0) == pdTRUE ? cmd : 0;
}

// Blocking connect (slirp answers with RST/EHOSTUNREACH immediately when no
// renderer is listening). Returns a socket or -1, with the failing errno left
// in *out_errno.
static int connect_to_renderer(int *out_errno) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    *out_errno = errno;
    return -1;
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(CONFIG_OBG_SIM_PORT);
  addr.sin_addr.s_addr = inet_addr(CONFIG_OBG_SIM_HOST);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    *out_errno = errno;
    close(sock);
    return -1;
  }
  return sock;
}

static void link_task(void *arg) {
  uint8_t out[sizeof(FRAME_MAGIC) + SIM_LINK_FRAME_BYTES];
  memcpy(out, FRAME_MAGIC, sizeof(FRAME_MAGIC));

  int sock = -1;
  int backoff_ms = RECONNECT_MIN_MS;
  uint32_t sent_seq = 0;
  int failures = 0;

  while (true) {
    if (sock < 0) {
      int err = 0;
      sock = connect_to_renderer(&err);
      if (sock < 0) {
        failures++;
        if (failures == 1 || failures % 10 == 0) {
          ESP_LOGW(TAG, "connect to %s:%d failed (errno %d), retrying",
                   CONFIG_OBG_SIM_HOST, CONFIG_OBG_SIM_PORT, err);
        }
        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        backoff_ms *= 2;
        if (backoff_ms > RECONNECT_MAX_MS) {
          backoff_ms = RECONNECT_MAX_MS;
        }
        continue;
      }
      failures = 0;
      ESP_LOGI(TAG, "renderer connected at %s:%d", CONFIG_OBG_SIM_HOST,
               CONFIG_OBG_SIM_PORT);
      backoff_ms = RECONNECT_MIN_MS;

      // Force a resend of the frame we already have.
      taskENTER_CRITICAL(&s_frame_lock);
      sent_seq = s_seq - 1;
      taskEXIT_CRITICAL(&s_frame_lock);
    }

    // Wait for a new frame or the pacing interval.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SEND_INTERVAL_MS));

    // Snapshot the mailbox if there is something newer than what we sent.
    uint32_t seq;
    taskENTER_CRITICAL(&s_frame_lock);
    seq = s_seq;
    if (seq != sent_seq) {
      memcpy(out + sizeof(FRAME_MAGIC), s_frame, SIM_LINK_FRAME_BYTES);
    }
    taskEXIT_CRITICAL(&s_frame_lock);

    if (seq != sent_seq) {
      ssize_t n = send(sock, out, sizeof(out), MSG_DONTWAIT);
      if (n == (ssize_t)sizeof(out)) {
        sent_seq = seq;
      } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // Renderer is slow; retry the same frame on the next tick.
      } else if (n < 0) {
        ESP_LOGW(TAG, "renderer send failed (%d), reconnecting", errno);
        close(sock);
        sock = -1;
        continue;
      }
      // A short send is not expected for 260 bytes; retry the frame.
    }

    // Drain any button commands from the renderer.
    bool disconnected = false;
    uint8_t cmd;
    while (!disconnected) {
      ssize_t n = recv(sock, &cmd, 1, MSG_DONTWAIT);
      if (n == 1) {
        if (cmd == 'S' || cmd == 'D' || cmd == 'L') {
          xQueueSend(s_commands, &cmd, 0);
        }
        continue;
      }
      if (n == 0) {
        ESP_LOGI(TAG, "renderer closed the connection");
        disconnected = true;
      } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        ESP_LOGW(TAG, "renderer recv failed (%d)", errno);
        disconnected = true;
      } else {
        break; // no data pending
      }
    }
    if (disconnected) {
      close(sock);
      sock = -1;
    }
  }
}
