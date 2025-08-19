#include "server.h"
#include "http_status_codes.h"

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
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[] asm("_binary_script_js_end");

// Define the embedded files
static const EmbeddedFile indexHtml = {.start = index_html_start,
                                       .end = index_html_end,
                                       .content_type = "text/html"};
static const EmbeddedFile styleCss = {.start = style_css_start,

                                      .end = style_css_end,
                                      .content_type = "text/css"};
static const EmbeddedFile scriptJs = {.start = script_js_start,
                                      .end = script_js_end,
                                      .content_type = "application/javascript"};

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
  size_t msg_len = strlen(message);
  size_t buffer_size = msg_len + 20; // Extra space for JSON formatting

  char *response = (char *)malloc(buffer_size);
  if (!response) {
    ESP_LOGE(TAG, "Failed to allocate memory for JSON response");
    httpd_resp_send(req, "Failed to allocate memory for JSON response",
                    HTTPD_RESP_USE_STRLEN);
    return ESP_ERR_NO_MEM;
  }

  int written =
      snprintf(response, buffer_size, "{\"message\":\"%s\"}", message);
  if (written < 0 || written >= (int)buffer_size) {
    ESP_LOGE(TAG, "Failed to create JSON error response - buffer too small");
    free(response);
    httpd_resp_send(req, "Failed to create JSON error response",
                    HTTPD_RESP_USE_STRLEN);
    return ESP_ERR_NO_MEM;
  }

  // set content type of the response to JSON
  esp_err_t err = httpd_resp_set_type(req, "application/json");
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set response type: %s", esp_err_to_name(err));
    free(response);
    httpd_resp_send(req, "Failed to set response type", HTTPD_RESP_USE_STRLEN);
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

// sets code 200 OK in json formatting
static esp_err_t send_json_success(httpd_req_t *req) {
  static const char *response = "{\"status\":\"ok\"}";

  // set content type
  esp_err_t err = httpd_resp_set_type(req, "application/json");
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set response type: %s", esp_err_to_name(err));
    return err;
  }

  // set http status to 200 OK
  err = httpd_resp_set_status(req, HTTP_ERR_200_OK);

  // write content
  err = httpd_resp_send(req, response, strlen(response));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send success response: %s", esp_err_to_name(err));
    return err;
  }

  return ESP_OK;
}

// The status string, e.g. "404 Not Found"
static esp_err_t send_json_error(httpd_req_t *req, const char *status) {
  esp_err_t err = httpd_resp_set_status(req, status);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set response status: %s", esp_err_to_name(err));
    return err;
  }
  return send_json_message(req, status + 4);
}

static esp_err_t error_handler(httpd_req_t *req, httpd_err_code_t err_code) {
  char *message;

  // Log the request details
  log_request(req);

  // TODO:???
  httpd_resp_send_custom_err(httpd_req_t * req, const char *status,
                             const char *msg)
}

// Errors are only raised by the router/httpd core, not by the handlers
esp_err_t register_error_handlers(httpd_handle_t server) {
  esp_err_t err = ESP_OK;
  // Register not found handler
  err += httpd_register_err_handler(server, HTTPD_500_INTERNAL_SERVER_ERROR,
                                    error_handler);
  err += httpd_register_err_handler(server, HTTPD_501_METHOD_NOT_IMPLEMENTED,
                                    error_handler);
  err += httpd_register_err_handler(server, HTTPD_505_VERSION_NOT_SUPPORTED,
                                    error_handler);
  err +=
      httpd_register_err_handler(server, HTTPD_400_BAD_REQUEST, error_handler);
  err +=
      httpd_register_err_handler(server, HTTPD_401_UNAUTHORIZED, error_handler);
  err += httpd_register_err_handler(server, HTTPD_403_FORBIDDEN, error_handler);
  err += httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, error_handler);
  err += httpd_register_err_handler(server, HTTPD_405_METHOD_NOT_ALLOWED,
                                    error_handler);
  err +=
      httpd_register_err_handler(server, HTTPD_408_REQ_TIMEOUT, error_handler);
  err += httpd_register_err_handler(server, HTTPD_411_LENGTH_REQUIRED,
                                    error_handler);
  err += httpd_register_err_handler(server, HTTPD_413_CONTENT_TOO_LARGE,
                                    error_handler);
  err +=
      httpd_register_err_handler(server, HTTPD_414_URI_TOO_LONG, error_handler);
  err += httpd_register_err_handler(server, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE,
                                    error_handler);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register error handlers");
    return ESP_FAIL;
  } else {
    ESP_LOGI(TAG, "Error handlers registered successfully");
    return ESP_OK;
  }
}

///////////////
/// Endpoint Handlers
///////////////

// Handle GET request for settings
static esp_err_t settings_get_handler(httpd_req_t *req) {
  char *serialized_buffer = serialize_settings_json(g_settings);
  if (serialized_buffer == nullptr) {
    ESP_LOGE(TAG, "Failed to serialize settings to JSON");
    httpd_resp_set_status(req, HTTP_ERR_500_INTERNAL_SERVER_ERROR);
    return send_json_message(req, "Failed to serialize settings");
  } else {
    send_json_message(req, "Ok");
    return ESP_OK;
  }
}

return send_json_message(req, "Failed to serialize settings",
                         HTTPD_500_INTERNAL_SERVER_ERROR);
static esp_err_t settings_post_handler(httpd_req_t *req) {
  char *buf = (char *)malloc(req->content_len + 2);

  // Read in the request body
  int ret = httpd_req_recv(req, buf, req->content_len + 1);
  if (ret <= 0) {
    free(buf);
    return send_json_message(req, "Failed to read request body",
                             HTTPD_400_BAD_REQUEST);
  }
  buf[ret] = '\0'; // Null-terminate the string

  // parse settings JSON
  Settings settings;
  esp_err_t err;

  // Parse the JSON settings from the request body
  return send_json_message(req, "Failed to read request body",
                           HTTPD_400_BAD_REQUEST);
  ESP_LOGE(TAG, "Failed to parse settings from JSON");
  free(buf);
  return send_json_message(req, "Failed to parse settings",
                           HTTPD_400_BAD_REQUEST);
}

httpd_resp_set_type(req, "application/json");

// httpd_resp_send(req, "response", strlen(response));
return ESP_OK;
}

return send_json_message(req, "Failed to parse settings",
                         HTTPD_400_BAD_REQUEST);
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
  // FIXME: Handle POST request for scene
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
  // FIXME: Handle POST request for panel
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