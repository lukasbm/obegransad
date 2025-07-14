#include "server.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <sys/stat.h>

const char *TAG = "server";

///////////////
/// HTTP Status Codes
///////////////

typedef struct {
  int code;
  const char *status;
} http_status_t;

static const http_status_t HTTP_STATUS_OK = {200, "200 OK"};
static const http_status_t HTTP_STATUS_CREATED = {201, "201 Created"};
static const http_status_t HTTP_STATUS_NO_CONTENT = {204, "204 No Content"};
static const http_status_t HTTP_STATUS_FOUND = {302, "302 Found"};
static const http_status_t HTTP_STATUS_BAD_REQUEST = {400, "400 Bad Request"};
static const http_status_t HTTP_STATUS_NOT_FOUND = {404, "404 Not Found"};
static const http_status_t HTTP_STATUS_INTERNAL_ERROR = {
    500, "500 Internal Server Error"};

static inline esp_err_t set_status(httpd_req_t *req,
                                   const http_status_t &status) {
  return httpd_resp_set_status(req, status.status);
}

///////////////
/// Embedded files
///////////////

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");

static esp_err_t serve_embedded_file(httpd_req_t *req, const uint8_t *start,
                                     const uint8_t *end,
                                     const char *content_type) {
  const size_t file_size = end - start;

  // Set content type
  httpd_resp_set_type(req, content_type);

  // Send file
  return httpd_resp_send(req, (const char *)start, file_size);
}

///////////////
/// Handlers
///////////////

static esp_err_t not_found_handler(httpd_req_t *req) {
  set_status(req, HTTP_STATUS_NOT_FOUND);
  // FIXME: filename!!!
  return serve_embedded_file(req, index_html_start, index_html_end,
                             "text/html");
}

static esp_err_t catch_all_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "Catch-all handler for URI: %s", req->uri);

  // TODO: check if its api, then return json 404
  // else return 404page.html

  // Check if it's a static asset request
  if (strstr(req->uri, "/assets/") != NULL) {
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Asset not found", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  // Redirect to root for SPA-style routing
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

// Wildcard route (must be registered last)
static const httpd_uri_t catch_all = {.uri = "*",
                                      .method = HTTP_GET,
                                      .handler = catch_all_handler,
                                      .user_ctx = NULL};

static esp_err_t settings_get_handler(httpd_req_t *req) {
  // Handle GET request for settings
  const char *response = "{\"status\":\"ok\"}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

static esp_err_t settings_post_handler(httpd_req_t *req) {
  // Handle POST request for settings
  char buf[100];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0) {
    return ESP_FAIL; // Error or no data received
  }
  buf[ret] = '\0'; // Null-terminate the string

  // Process the received data (e.g., save settings)
  ESP_LOGI(TAG, "Received settings: %s", buf);

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
  return serve_embedded_file(req, index_html_start, index_html_end,
                             "text/html");
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

static const httpd_uri_t settings_get = {.uri = "/settings",
                                         .method = HTTP_GET,
                                         .handler = settings_get_handler,
                                         .user_ctx = NULL};
static const httpd_uri_t settings_post = {.uri = "/settings",
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

///////////////
//// Actual Server Setup
////////////////

static esp_err_t register_routes(httpd_handle_t &server) {
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &root_get), TAG,
                      "Failed to register root handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_get), TAG,
                      "Failed to register settings GET handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_post), TAG,
                      "Failed to register settings POST handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_delete), TAG,
                      "Failed to register settings DELETE handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &scene_post), TAG,
                      "Failed to register scene POST handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &panel_post), TAG,
                      "Failed to register panel POST handler");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &panel_get), TAG,
                      "Failed to register panel GET handler");
  // ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &not_found_handler),
  //                     TAG, "Failed to register not found handler");

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

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  ESP_RETURN_ON_ERROR(httpd_start(&g_server, &config), TAG,
                      "Failed to start web server");
  return register_routes(g_server);
}

esp_err_t stop_webserver() {
  if (g_server == nullptr) {
    ESP_LOGI(TAG, "Web server not running");
    return ESP_OK; // Server not started
  }

  httpd_stop(g_server);
  g_server = nullptr; // Reset server handle
  return ESP_OK;
}
