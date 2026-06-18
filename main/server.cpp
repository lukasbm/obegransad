#include "server.h"
#include "http_status_codes.h"

#include "app_events.h"
#include "config.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>
#include <stdlib.h>

static const char *TAG = "server";

///////////////
/// Embedded files
///////////////

struct EmbeddedFile {
  const uint8_t *start;
  const uint8_t *end;
  const char *content_type;
};

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

// The config page is a single self-contained file (inline CSS + JS).
static const EmbeddedFile indexHtml = {.start = index_html_start,
                                       .end = index_html_end,
                                       .content_type = "text/html"};

// The caller has to set the http status
// start and end refer to the asm pointers of the embedded files
static esp_err_t serve_embedded_file(httpd_req_t *req,
                                     const EmbeddedFile &file) {
  const size_t file_size = file.end - file.start;

  // Set content type
  esp_err_t err = httpd_resp_set_type(req, file.content_type);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set content type: %s", esp_err_to_name(err));
    return err;
  }

  // Send file
  err = httpd_resp_send(req, (const char *)file.start, file_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send embedded file: %s", esp_err_to_name(err));
    return err;
  }

  return ESP_OK; // Return success if everything went well
}

//////////////
// Error handlers
// These are only for the integrated error cases (e.g. Method not allowed,
// timeout, route not found, etc.) They are called automatically
///////////////

static void log_request(httpd_req_t *req) {
  ESP_LOGE(TAG, "Request method: %d", req->method);
  ESP_LOGE(TAG, "Request URI: %s", req->uri);
  // for (int i = 0; i < req->hdrs_count; i++) {
  //   ESP_LOGE(TAG, "Header %d: %s: %s", i, req->hdrs[i].key,
  //   req->hdrs[i].value);
  // }
}

// Default function to send a general JSON error message of the form
// {"message":"<message>"}
// This is used for API endpoints that expect JSON responses
static esp_err_t send_json_message(httpd_req_t *req, const char *message) {
  size_t buffer_size = strlen(message) + 20; // Extra space for JSON formatting
  char *response = (char *)malloc(buffer_size);
  if (!response) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON response");
    httpd_resp_send(
        req, "{\"message\":\"Failed to allocate memory for JSON response\"}",
        HTTPD_RESP_USE_STRLEN);
    return ESP_ERR_NO_MEM;
  }

  int written =
      snprintf(response, buffer_size, "{\"message\":\"%s\"}", message);
  if (written < 0 || written >= (int)buffer_size) {
    ESP_LOGE(TAG, "Failed to create JSON error response - buffer too small");
    free(response);
    httpd_resp_send(req,
                    "{\"message\":\"Failed to create JSON error response\"}",
                    HTTPD_RESP_USE_STRLEN);
    return ESP_ERR_NO_MEM;
  }

  // set content type of the response to JSON
  esp_err_t err = httpd_resp_set_type(req, "application/json");
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set response type: %s", esp_err_to_name(err));
    free(response);
    httpd_resp_send(req, "{\"message\":\"Failed to set response type\"}",
                    HTTPD_RESP_USE_STRLEN);
    return err;
  }

  err = httpd_resp_send(req, response, strlen(response));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send error response: %s", esp_err_to_name(err));
    free(response);
    return err;
  }

  free(response);
  return ESP_OK;
}

// The status string, e.g. "404 Not Found"
static esp_err_t send_json_status(httpd_req_t *req, const char *status) {
  // set status message
  esp_err_t err = httpd_resp_set_status(req, status);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set response status: %s", esp_err_to_name(err));
    // FIXME: send something?
    return err;
  }

  // send with correct content type
  return send_json_message(req, status + 4);
}

// generic handler for all errors
// only raised by the router/httpd core, not by the handlers
static esp_err_t error_handler(httpd_req_t *req, httpd_err_code_t err_code) {
  if (strstr(req->uri, "/api/") != nullptr) {
    // For API endpoints, send JSON error response
    const char *message = convert_httpd_err_code_to_string(err_code);
    return send_json_status(req, message);
  } else {
    // For non-API endpoints, send HTML error response
    return httpd_resp_send_err(req, err_code, nullptr);
  }
}

// Errors are only raised by the router/httpd core, not by the handlers
// registering all avaiable in httpd_err_code_t
esp_err_t register_error_handlers(httpd_handle_t server) {
  // 500
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_500_INTERNAL_SERVER_ERROR,
                                 error_handler),
      TAG, "Failed to register 500 error handler");

  // 501
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_501_METHOD_NOT_IMPLEMENTED,
                                 error_handler),
      TAG, "Failed to register 501 error handler");

  // 505
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_505_VERSION_NOT_SUPPORTED,
                                 error_handler),
      TAG, "Failed to register 505 error handler");

  // 400
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_400_BAD_REQUEST, error_handler),
      TAG, "Failed to register 400 error handler");

  // 401
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_401_UNAUTHORIZED, error_handler),
      TAG, "Failed to register 401 error handler");

  // 403
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_403_FORBIDDEN, error_handler),
      TAG, "Failed to register 403 error handler");

  // 404
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, error_handler),
      TAG, "Failed to register 404 error handler");

  // 405
  ESP_RETURN_ON_ERROR(httpd_register_err_handler(
                          server, HTTPD_405_METHOD_NOT_ALLOWED, error_handler),
                      TAG, "Failed to register 405 error handler");

  // 408
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_408_REQ_TIMEOUT, error_handler),
      TAG, "Failed to register 408 error handler");

  // 411
  ESP_RETURN_ON_ERROR(httpd_register_err_handler(
                          server, HTTPD_411_LENGTH_REQUIRED, error_handler),
                      TAG, "Failed to register 411 error handler");

  // 413
  ESP_RETURN_ON_ERROR(httpd_register_err_handler(
                          server, HTTPD_413_CONTENT_TOO_LARGE, error_handler),
                      TAG, "Failed to register 413 error handler");

  // 414
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_414_URI_TOO_LONG, error_handler),
      TAG, "Failed to register 414 error handler");

  // 431
  ESP_RETURN_ON_ERROR(
      httpd_register_err_handler(server, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE,
                                 error_handler),
      TAG, "Failed to register 431 error handler");

  return ESP_OK;
}

///////////////
/// Endpoint Handlers
///////////////

// Handle GET request for settings
// This function returns the current settings in JSON format
static esp_err_t settings_get_handler(httpd_req_t *req) {
  char *serialized_buffer = serialize_settings_json(g_settings);
  if (serialized_buffer == nullptr) {
    ESP_LOGE(TAG, "Failed to serialize settings to JSON");
    httpd_resp_set_status(req, HTTP_ERR_500_INTERNAL_SERVER_ERROR);
    return send_json_message(req, "Failed to serialize settings");
  } else {
    // send out buffer
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, serialized_buffer, strlen(serialized_buffer));
  }
}

// Handle POST request for settings
// This function expects a JSON body with the new settings
static esp_err_t settings_post_handler(httpd_req_t *req) {
  char *buf = (char *)malloc(req->content_len + 2);
  if (buf == nullptr) {
    httpd_resp_set_status(req, HTTP_ERR_500_INTERNAL_SERVER_ERROR);
    return send_json_message(req, "Out of memory");
  }

  // Read in the request body
  int ret = httpd_req_recv(req, buf, req->content_len + 1);
  if (ret <= 0) {
    free(buf);
    httpd_resp_set_status(req, HTTP_ERR_400_BAD_REQUEST);
    return send_json_message(req, "Failed to read request body");
  }
  buf[ret] = '\0'; // Null-terminate the string

  // parse settings JSON
  Settings settings;

  esp_err_t err = parse_settings_json(buf, settings);
  if (err != ESP_OK) {
    httpd_resp_set_status(req, HTTP_ERR_400_BAD_REQUEST);
    free(buf);
    return send_json_message(req, "Failed to parse settings JSON");
  }

  // Update global settings and persist to NVS
  g_settings = settings;
  free(buf);

  esp_err_t persist_err = nvs_write_settings(g_settings);
  if (persist_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to persist settings: %s",
             esp_err_to_name(persist_err));
    httpd_resp_set_status(req, HTTP_ERR_500_INTERNAL_SERVER_ERROR);
    return send_json_message(req, "Settings parsed but failed to persist");
  }

  // Let the application re-apply settings that don't take effect on their own
  // (timezone, weather location) on the main task.
  app_post_event(APP_EVT_SETTINGS_CHANGED);

  ESP_LOGI(TAG, "Settings updated and persisted successfully");
  return send_json_message(req, "Settings updated successfully");
}

// Handle DELETE request for settings
// This function resets the settings to defaults
static esp_err_t settings_delete_handler(httpd_req_t *req) {
httpd_resp_set_status(req, HTTP_ERR_501_NOT_IMPLEMENTED);
  return send_json_message(req, "Settings reset not implemented yet");
}

///////////////
//// Define Routes
////////////////

static esp_err_t catch_all_handler(httpd_req_t *req) {
  return send_json_status(req, HTTP_ERR_404_NOT_FOUND);
}

// Serve the config page at the root.
static esp_err_t root_get_handler(httpd_req_t *req) {
  return serve_embedded_file(req, indexHtml);
}

static const httpd_uri_t root_get = {.uri = "/",
                                     .method = HTTP_GET,
                                     .handler = root_get_handler,
                                     .user_ctx = NULL};

static const httpd_uri_t settings_get = {.uri = "/api/settings",
                                         .method = HTTP_GET,
                                         .handler = settings_get_handler,
                                         .user_ctx = NULL};
static const httpd_uri_t settings_post = {.uri = "/api/settings",
                                          .method = HTTP_POST,
                                          .handler = settings_post_handler,
                                          .user_ctx = NULL};
static const httpd_uri_t settings_delete = {.uri = "/settings",
                                            .method = HTTP_DELETE,
                                            .handler = settings_delete_handler,
                                            .user_ctx = NULL};

static const httpd_uri_t catch_all = {
    .uri = "/*", // Catch all routes
    .method = (httpd_method_t)HTTP_ANY,
    .handler = catch_all_handler,
    .user_ctx = NULL};

///////////////
//// Actual Server Setup
////////////////

static esp_err_t register_routes(httpd_handle_t &server) {
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &root_get), TAG,
                      "Failed to register root GET handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_get), TAG,
                      "Failed to register settings GET handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_post), TAG,
                      "Failed to register settings POST handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_delete), TAG,
                      "Failed to register settings DELETE handler");

  // Register catch-all handler last
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &catch_all), TAG,
                      "Failed to register catch-all handler");

  return ESP_OK;
}

httpd_handle_t g_server = nullptr;

esp_err_t start_webserver() {
  if (g_server != nullptr) {
    ESP_LOGI(TAG, "Web server already running");
    return ESP_OK; // Server already started
  }

  httpd_handle_t server;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  // The catch-all "/*" handler needs the wildcard matcher enabled.
  config.uri_match_fn = httpd_uri_match_wildcard;
  ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG,
                      "Failed to start web server");
  ESP_RETURN_ON_ERROR(register_error_handlers(server), TAG,
                      "Failed to register error handlers");
  ESP_RETURN_ON_ERROR(register_routes(server), TAG,
                      "Failed to register routes");
  g_server = server; // Store server handle globally
  ESP_LOGI(TAG, "Web server started successfully");
  return ESP_OK;
}

esp_err_t stop_webserver() {
  if (g_server == nullptr) {
    ESP_LOGI(TAG, "Web server not running");
    return ESP_OK; // Server not started
  }

  esp_err_t err = httpd_stop(g_server);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to stop web server: %s", esp_err_to_name(err));
    return err; // Error stopping server
  } else {
    g_server = nullptr; // Reset server handle
    return ESP_OK;
  }
}