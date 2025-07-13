#include "server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <sys/stat.h>

const char *TAG = "server";

///////////////
/// Handlers
///////////////
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

static esp_err_t root_get_handler(httpd_req_t *req) { return ESP_OK; }

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

///////////////
//// Other Stuff
////////////////

httpd_handle_t start_webserver() {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  if (httpd_start(&server, &config) == ESP_OK) {
    ESP_LOGI(TAG, "Server started successfully, registering URI handlers...");
    return server;
  }

  ESP_LOGE(TAG, "Failed to start server");
  return NULL;
}

static void register_routes(httpd_handle_t &server) {
  httpd_register_uri_handler(server, &settings_get);
}
