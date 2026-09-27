#include "ota.h"

#include "device.h"   // wifi_check()
#include "helper.hpp" // millis()
#include "sdkconfig.h"

#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <esp_log.h>
#include <esp_system.h>

#include <cJSON.h>

static const char *TAG = "ota";

static constexpr size_t BUF_SIZE = 4096;
static constexpr uint32_t RESTART_DELAY_MS = 800;

// Provenance: only images built from this project are accepted.
static constexpr const char *EXPECTED_PROJECT = "obegransad";

static bool token_configured(void) { return CONFIG_OBG_OTA_TOKEN[0] != '\0'; }

// Constant-time comparison of the request token against the configured one.
static bool token_matches(const char *candidate) {
  const char *expected = CONFIG_OBG_OTA_TOKEN;
  const size_t expected_len = strlen(expected);
  if (candidate == nullptr || strlen(candidate) != expected_len) {
    return false;
  }
  unsigned char diff = 0;
  for (size_t i = 0; i < expected_len; i++) {
    diff |= (unsigned char)(expected[i] ^ candidate[i]);
  }
  return diff == 0;
}

// --- JSON helpers (kept local so ota.cpp does not depend on config_server) ---

static esp_err_t ota_send_json(httpd_req_t *req, cJSON *root) {
  char *body = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (body == nullptr) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "JSON encoding failed");
  }
  httpd_resp_set_type(req, "application/json");
  const esp_err_t err = httpd_resp_sendstr(req, body);
  free(body);
  return err;
}

static esp_err_t ota_send_error(httpd_req_t *req, const char *status,
                                const char *message) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "error", message);
  char *body = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  const esp_err_t err = httpd_resp_sendstr(req, body ? body : "{}");
  free(body);
  return err;
}

// -----------------------------------------------------------------------------
// Linux/POSIX host build: there are no partitions or flash, so OTA is not
// available. The endpoints still answer so the API surface stays identical.
// -----------------------------------------------------------------------------

#if CONFIG_IDF_TARGET_LINUX

esp_err_t ota_init(void) { return ESP_OK; }
void ota_process(void) {}

esp_err_t ota_handle_get(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "state", "idle");
  cJSON_AddBoolToObject(root, "supported", false);
  cJSON_AddBoolToObject(root, "auth_required", CONFIG_OBG_OTA_TOKEN[0] != '\0');
  cJSON_AddNullToObject(root, "running");
  cJSON_AddNullToObject(root, "next");
  cJSON_AddNumberToObject(root, "received", 0);
  cJSON_AddNumberToObject(root, "total", 0);
  cJSON_AddNullToObject(root, "last_error");
  cJSON_AddBoolToObject(root, "restart_pending", false);
  return ota_send_json(req, root);
}

esp_err_t ota_handle_post(httpd_req_t *req) {
  return ota_send_error(req, "501 Not Implemented",
                        "OTA is not available on the host target");
}

#else // !CONFIG_IDF_TARGET_LINUX

#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

enum class OtaState { IDLE, RECEIVING, READY, ERROR };

static std::atomic<OtaState> s_state{OtaState::IDLE};
static std::atomic<uint32_t> s_received{0};
static std::atomic<uint32_t> s_total{0};
static std::atomic<uint32_t> s_restart_at_ms{0};
static char s_error[64] = {};
static uint8_t *s_buf = nullptr;

static const char *state_name(OtaState state) {
  switch (state) {
  case OtaState::IDLE:
    return "idle";
  case OtaState::RECEIVING:
    return "receiving";
  case OtaState::READY:
    return "ready";
  case OtaState::ERROR:
    return "error";
  }
  return "unknown";
}

static void reset_transfer(void) {
  s_state.store(OtaState::IDLE);
  s_received.store(0);
  s_total.store(0);
}

esp_err_t ota_init(void) {
  // Confirm the running image as soon as possible: the long-press factory
  // reset calls esp_restart(), and a reset while an OTA image is still
  // PENDING_VERIFY would silently roll the update back.
  const esp_err_t valid = esp_ota_mark_app_valid_cancel_rollback();
  if (valid != ESP_OK) {
    ESP_LOGW(TAG, "mark_app_valid: %s", esp_err_to_name(valid));
  }

  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  esp_app_desc_t desc = {};
  if (running != nullptr &&
      esp_ota_get_partition_description(running, &desc) == ESP_OK) {
    ESP_LOGI(TAG, "running %s at 0x%08" PRIx32 ", version %s (boot %s)",
             running->label, running->address, desc.version,
             boot ? boot->label : "?");
  }

  s_buf = (uint8_t *)malloc(BUF_SIZE);
  if (s_buf == nullptr) {
    ESP_LOGE(TAG, "receive buffer allocation failed");
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

void ota_process(void) {
  const uint32_t at = s_restart_at_ms.load();
  if (at != 0 && (uint32_t)millis() >= at) {
    s_restart_at_ms.store(0);
    ESP_LOGI(TAG, "restarting into the new image");
    esp_restart();
  }
}

static void add_partition_json(cJSON *root, const char *name,
                               const esp_partition_t *partition) {
  if (partition == nullptr) {
    cJSON_AddNullToObject(root, name);
    return;
  }
  cJSON *obj = cJSON_AddObjectToObject(root, name);
  cJSON_AddStringToObject(obj, "label", partition->label);
  cJSON_AddNumberToObject(obj, "offset", (double)partition->address);
  if (strcmp(name, "running") == 0) {
    esp_app_desc_t desc = {};
    if (esp_ota_get_partition_description(partition, &desc) == ESP_OK) {
      cJSON_AddStringToObject(obj, "version", desc.version);
    }
  }
}

esp_err_t ota_handle_get(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "state", state_name(s_state.load()));
  cJSON_AddBoolToObject(root, "supported", true);
  cJSON_AddBoolToObject(root, "auth_required", token_configured());
  add_partition_json(root, "running", esp_ota_get_running_partition());
  add_partition_json(root, "next", esp_ota_get_next_update_partition(nullptr));
  cJSON_AddNumberToObject(root, "received", s_received.load());
  cJSON_AddNumberToObject(root, "total", s_total.load());
  if (s_error[0] != '\0') {
    cJSON_AddStringToObject(root, "last_error", s_error);
  } else {
    cJSON_AddNullToObject(root, "last_error");
  }
  cJSON_AddBoolToObject(root, "restart_pending", s_restart_at_ms.load() != 0);
  return ota_send_json(req, root);
}

esp_err_t ota_handle_post(httpd_req_t *req) {
  // OTA is only offered while connected as a station (not while offline or
  // during any future provisioning mode).
  if (!wifi_check()) {
    return ota_send_error(req, "403 Forbidden",
                          "OTA requires a station connection");
  }

  if (!token_configured()) {
    return ota_send_error(req, "403 Forbidden",
                          "OTA disabled: set CONFIG_OBG_OTA_TOKEN");
  }
  char token[80] = {};
  if (httpd_req_get_hdr_value_len(req, "X-OTA-Token") == 0 ||
      httpd_req_get_hdr_value_str(req, "X-OTA-Token", token, sizeof(token)) !=
          ESP_OK ||
      !token_matches(token)) {
    ESP_LOGW(TAG, "rejected OTA upload with invalid or missing token");
    return ota_send_error(req, "401 Unauthorized",
                          "invalid or missing X-OTA-Token");
  }

  const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
  if (target == nullptr) {
    return ota_send_error(req, "501 Not Implemented",
                          "no OTA partition available");
  }
  if (s_state.load() == OtaState::RECEIVING) {
    return ota_send_error(req, "409 Conflict", "upload already in progress");
  }
  if (req->content_len == 0) {
    return ota_send_error(req, "411 Length Required",
                          "Content-Length is required");
  }
  if (req->content_len > target->size) {
    return ota_send_error(req, "413 Payload Too Large",
                          "image is larger than the target partition");
  }

  esp_ota_handle_t handle = 0;
  esp_err_t err = esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &handle);
  if (err == ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
    return ota_send_error(req, "409 Conflict",
                          "running image has not been confirmed yet");
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
    return ota_send_error(req, "500 Internal Server Error",
                          "OTA setup failed");
  }

  s_state.store(OtaState::RECEIVING);
  s_received.store(0);
  s_total.store((uint32_t)req->content_len);
  s_error[0] = '\0';
  ESP_LOGI(TAG, "upload start: %u bytes -> %s", (unsigned)req->content_len,
           target->label);

  size_t remaining = req->content_len;
  size_t next_log = 64 * 1024;
  while (remaining > 0) {
    const size_t to_read = remaining < BUF_SIZE ? remaining : BUF_SIZE;
    const int received = httpd_req_recv(req, (char *)s_buf, to_read);
    if (received <= 0) {
      // Timeout or client gone: discard, the old image keeps booting.
      esp_ota_abort(handle);
      snprintf(s_error, sizeof(s_error), "transfer aborted");
      reset_transfer();
      s_state.store(OtaState::ERROR);
      return ota_send_error(req, "400 Bad Request", "transfer aborted");
    }
    err = esp_ota_write(handle, s_buf, (size_t)received);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
      esp_ota_abort(handle);
      snprintf(s_error, sizeof(s_error), "flash write failed");
      reset_transfer();
      s_state.store(OtaState::ERROR);
      return ota_send_error(req, err == ESP_ERR_OTA_VALIDATE_FAILED
                                     ? "400 Bad Request"
                                     : "500 Internal Server Error",
                            err == ESP_ERR_OTA_VALIDATE_FAILED
                                ? "image validation failed"
                                : "flash write failed");
    }
    s_received.fetch_add((uint32_t)received);
    remaining -= (size_t)received;
    if (s_received.load() >= next_log) {
      ESP_LOGD(TAG, "progress %u/%u", (unsigned)s_received.load(),
               (unsigned)req->content_len);
      next_log += 64 * 1024;
    }
  }
  ESP_LOGI(TAG, "upload complete, verifying");

  err = esp_ota_end(handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "image validation failed: %s", esp_err_to_name(err));
    snprintf(s_error, sizeof(s_error), "image validation failed");
    reset_transfer();
    s_state.store(OtaState::ERROR);
    return ota_send_error(req, "400 Bad Request", "image validation failed");
  }

  // Provenance: the image is structurally valid, but is it ours? Refuse to
  // switch the boot partition for a foreign project's image; the inactive slot
  // may keep it, otadata is untouched, the running image keeps booting.
  esp_app_desc_t desc = {};
  if (esp_ota_get_partition_description(target, &desc) != ESP_OK) {
    snprintf(s_error, sizeof(s_error), "cannot read image description");
    reset_transfer();
    s_state.store(OtaState::ERROR);
    return ota_send_error(req, "400 Bad Request", "cannot read image description");
  }
  if (strcmp(desc.project_name, EXPECTED_PROJECT) != 0) {
    ESP_LOGW(TAG, "rejecting image for project '%s'", desc.project_name);
    snprintf(s_error, sizeof(s_error), "not an %s image", EXPECTED_PROJECT);
    reset_transfer();
    s_state.store(OtaState::ERROR);
    return ota_send_error(req, "400 Bad Request", "not an obegransad image");
  }

  err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "set_boot_partition failed: %s", esp_err_to_name(err));
    snprintf(s_error, sizeof(s_error), "boot partition switch failed");
    reset_transfer();
    s_state.store(OtaState::ERROR);
    return ota_send_error(req, "500 Internal Server Error",
                          "boot partition switch failed");
  }

  const char *version = desc.version;
  s_state.store(OtaState::READY);
  s_restart_at_ms.store((uint32_t)millis() + RESTART_DELAY_MS);
  ESP_LOGI(TAG, "image accepted (%s), boot slot %s, restarting", version,
           target->label);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "success", true);
  cJSON_AddStringToObject(root, "version", version);
  cJSON_AddStringToObject(root, "slot", target->label);
  cJSON_AddBoolToObject(root, "restarting", true);
  httpd_resp_set_status(req, "202 Accepted");
  return ota_send_json(req, root);
}

#endif // !CONFIG_IDF_TARGET_LINUX
