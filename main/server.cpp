#include "server.h"

#include "config.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

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
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");

// Define the embedded files
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

// Default function to send a general JSON error message of the form
// {"message":"<error_message>"}
// This is used for API endpoints that expect JSON responses
static esp_err_t send_json_error_message(httpd_req_t *req,
                                         const char *error_message,
                                         const httpd_err_code_t status) {
  char response[256]; // Max 256 bytes for JSON response
  int written = snprintf(response, sizeof(response), "{\"message\":\"%s\"}",
                         error_message);
  if (written < 0 || written >= (int)sizeof(response)) {
    ESP_LOGE(TAG, "Failed to create JSON error response");
    return ESP_ERR_NO_MEM; // Not enough memory to create response
  }

  // set content type of the response to JSON
  esp_err_t err = httpd_resp_set_type(req, "application/json");
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set response type: %s", esp_err_to_name(err));
    return err;
  }

  // Set the HTTP status code and send the response
  err = httpd_resp_send_err(req, status, response);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send error response: %s", esp_err_to_name(err));
    return err;
  }

  return ESP_OK;
}

static char *get_error_message(const httpd_err_code_t error_code) {
  return "HI";
}

<template httpd_err_code_t ErrorCode> static esp_err_t
not_found_handler(httpd_req_t *req) {
  char *message = get_error_message(ErrorCode);
  ESP_LOGW(TAG, "Not found: %s", req->uri);
  if (strstr(req->uri, "/api/")) {
    return send_json_error_message(req, message, ErrorCode);
  } else {
    return httpd_resp_send_err(req, ErrorCode, message);
  }
}

esp_err_t register_error_handlers(httpd_handle_t server) {
  // Register not found handler
  esp_err_t err = httpd_register_err_handler(server, HTTPD_404_NOT_FOUND,
                                             not_found_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register not found handler");
    return err;
  }

  return ESP_OK;
}

///////////////
/// Endpoint Handlers
///////////////

// Handle GET request for settings
static esp_err_t settings_get_handler(httpd_req_t *req) {
  char serialized_buffer[1000];
  esp_err_t err = serialize_settings_json(serialized_buffer, g_settings);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to serialize settings to JSON");
    return send_json_error_message(req, "Failed to serialize settings",
                                   HTTPD_500_INTERNAL_SERVER_ERROR);
  } else {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, serialized_buffer, strlen(serialized_buffer));
    return ESP_OK;
  }
}

// Handle POST request for settings
static esp_err_t settings_post_handler(httpd_req_t *req) {
  char buf[1000]; // max 1000 bytes for settings JSON
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0) {
    return ESP_FAIL; // Error or no data received
  }
  buf[ret] = '\0'; // Null-terminate the string

  // parse settings JSON
  Settings settings;
  ESP_RETURN_ON_ERROR(parse_settings_json(buf, settings), TAG,
                      "Failed to parse settings from JSON");

  httpd_resp_set_type(req, "application/json");

  // httpd_resp_send(req, "response", strlen(response));
  return ESP_OK;
}

static esp_err_t sleep_start_handler(httpd_req_t *req) {
  // Handle POST request to start sleep mode
  ESP_LOGI(TAG, "Starting light sleep mode");

  // Set up light sleep (e.g., enable wakeup sources)
  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 1); // Example for GPIO wakeup

  // TODO: Enter light sleep
  // esp_light_sleep_start();

  const char *response = "{\"status\":\"ok\"}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

static esp_err_t settings_delete_handler(httpd_req_t *req) {
  // Handle DELETE request for settings
  // Here you would typically reset settings to defaults
  ESP_LOGI(TAG, "Settings reset to defaults");

  const char *response = "{\"status\":\"ok\"}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req) {
  // return serve_embedded_file(req, index_html_start, index_html_end,
  //  "text/html");
  return ESP_OK;
}

static esp_err_t scene_post_handler(httpd_req_t *req) {
  // Handle POST request for scene
  char buf[100];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0) {
    return ESP_FAIL; // Error or no data received
  }
  buf[ret] = '\0'; // Null-terminate the string

  // Process the received data (e.g., apply scene)
  ESP_LOGI(TAG, "Received scene: %s", buf);

  const char *response = "{\"status\":\"ok\"}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

static esp_err_t panel_post_handler(httpd_req_t *req) {
  // Handle POST request for panel
  char buf[100];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0) {
    return ESP_FAIL; // Error or no data received
  }
  buf[ret] = '\0'; // Null-terminate the string

  // Process the received data (e.g., update panel)
  ESP_LOGI(TAG, "Received panel data: %s", buf);

  const char *response = "{\"status\":\"ok\"}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

static esp_err_t panel_get_handler(httpd_req_t *req) {
  // Handle GET request for panel
  const char *response = "{\"status\":\"ok\"}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

///////////////
//// Define Routes
////////////////

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
static const httpd_uri_t scene_post = {.uri = "/scene",
                                       .method = HTTP_POST,
                                       .handler = scene_post_handler,
                                       .user_ctx = NULL};
static const httpd_uri_t panel_post = {.uri = "/panel",
                                       .method = HTTP_POST,
                                       .handler = panel_post_handler,
                                       .user_ctx = NULL};
static const httpd_uri_t panel_get = {.uri = "/panel",
                                      .method = HTTP_GET,
                                      .handler = panel_get_handler,
                                      .user_ctx = NULL};
static const httpd_uri_t root_get = {.uri = "/",
                                     .method = HTTP_GET,
                                     .handler = root_get_handler,
                                     .user_ctx = NULL}; // Root handler
static const httpd_uri_t sleep_post = {.uri = "/sleep",
                                       .method = HTTP_POST,
                                       .handler = sleep_start_handler,
                                       .user_ctx = NULL};

///////////////
//// Actual Server Setup
////////////////

static esp_err_t register_routes(httpd_handle_t &server) {
  // ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &root_get), TAG,
  //                     "Failed to register root handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_get), TAG,
                      "Failed to register settings GET handler");
  // ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_post),
  // TAG,
  //                     "Failed to register settings POST handler");
  // ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_delete),
  // TAG,
  //                     "Failed to register settings DELETE handler");
  // ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &scene_post), TAG,
  //                     "Failed to register scene POST handler");
  // ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &panel_post), TAG,
  //                     "Failed to register panel POST handler");
  // ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &panel_get), TAG,
  //                     "Failed to register panel GET handler");
  // ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &not_found_handler),
  //                     TAG, "Failed to register not found handler");

  // Register catch-all handler last
  // ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &catch_all), TAG,
  //                     "Failed to register catch-all handler");

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
  ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG,
                      "Failed to start web server");
  // ESP_RETURN_ON_ERROR(register_error_handlers(server), TAG,
  //                     "Failed to register error handlers");
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