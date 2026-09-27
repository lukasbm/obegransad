#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

// Push OTA over the config server: the client POSTs a raw application image to
// /api/ota, the device writes the inactive slot, validates it, switches the
// boot partition and restarts. See docs/openapi.yaml and ota_plan.md.
//
// ota_handle_* run on the httpd task; ota_process() runs on the main task (the
// only place allowed to draw or restart).

esp_err_t ota_init(void);    // call once from app_main (after NVS is up)
void ota_process(void);      // call once per main-loop tick

esp_err_t ota_handle_get(httpd_req_t *req);  // GET  /api/ota
esp_err_t ota_handle_post(httpd_req_t *req); // POST /api/ota (octet-stream)
