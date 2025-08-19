#pragma once

#include "esp_http_server.h"

// HTTP status codes as defined in RFC 7231 and other related RFCs
#define HTTP_ERR_100_CONTINUE "100 Continue"
#define HTTP_ERR_101_SWITCHING_PROTOCOLS "101 Switching Protocols"
#define HTTP_ERR_102_PROCESSING "102 Processing"

#define HTTP_ERR_200_OK "200 OK"
#define HTTP_ERR_201_CREATED "201 Created"
#define HTTP_ERR_202_ACCEPTED "202 Accepted"
#define HTTP_ERR_203_NON_AUTHORITATIVE_INFORMATION                             \
  "203 Non-authoritative Information"
#define HTTP_ERR_204_NO_CONTENT "204 No Content"
#define HTTP_ERR_205_RESET_CONTENT "205 Reset Content"
#define HTTP_ERR_206_PARTIAL_CONTENT "206 Partial Content"
#define HTTP_ERR_207_MULTI_STATUS "207 Multi-Status"
#define HTTP_ERR_208_ALREADY_REPORTED "208 Already Reported"
#define HTTP_ERR_226_IM_USED "226 IM Used"

#define HTTP_ERR_300_MULTIPLE_CHOICES "300 Multiple Choices"
#define HTTP_ERR_301_MOVED_PERMANENTLY "301 Moved Permanently"
#define HTTP_ERR_302_FOUND "302 Found"
#define HTTP_ERR_303_SEE_OTHER "303 See Other"
#define HTTP_ERR_304_NOT_MODIFIED "304 Not Modified"
#define HTTP_ERR_305_USE_PROXY "305 Use Proxy"
#define HTTP_ERR_307_TEMPORARY_REDIRECT "307 Temporary Redirect"
#define HTTP_ERR_308_PERMANENT_REDIRECT "308 Permanent Redirect"

#define HTTP_ERR_400_BAD_REQUEST "400 Bad Request"
#define HTTP_ERR_401_UNAUTHORIZED "401 Unauthorized"
#define HTTP_ERR_402_PAYMENT_REQUIRED "402 Payment Required"
#define HTTP_ERR_403_FORBIDDEN "403 Forbidden"
#define HTTP_ERR_404_NOT_FOUND "404 Not Found"
#define HTTP_ERR_405_METHOD_NOT_ALLOWED "405 Method Not Allowed"
#define HTTP_ERR_406_NOT_ACCEPTABLE "406 Not Acceptable"
#define HTTP_ERR_407_PROXY_AUTHENTICATION_REQUIRED                             \
  "407 Proxy Authentication Required"
#define HTTP_ERR_408_REQUEST_TIMEOUT "408 Request Timeout"
#define HTTP_ERR_409_CONFLICT "409 Conflict"
#define HTTP_ERR_410_GONE "410 Gone"
#define HTTP_ERR_411_LENGTH_REQUIRED "411 Length Required"
#define HTTP_ERR_412_PRECONDITION_FAILED "412 Precondition Failed"
#define HTTP_ERR_413_PAYLOAD_TOO_LARGE "413 Payload Too Large"
#define HTTP_ERR_414_REQUEST_URI_TOO_LONG "414 Request-URI Too Long"
#define HTTP_ERR_415_UNSUPPORTED_MEDIA_TYPE "415 Unsupported Media Type"
#define HTTP_ERR_416_REQUESTED_RANGE_NOT_SATISFIABLE                           \
  "416 Requested Range Not Satisfiable"
#define HTTP_ERR_417_EXPECTATION_FAILED "417 Expectation Failed"
#define HTTP_ERR_421_MISDIRECTED_REQUEST "421 Misdirected Request"
#define HTTP_ERR_422_UNPROCESSABLE_ENTITY "422 Unprocessable Entity"
#define HTTP_ERR_423_LOCKED "423 Locked"
#define HTTP_ERR_424_FAILED_DEPENDENCY "424 Failed Dependency"
#define HTTP_ERR_426_UPGRADE_REQUIRED "426 Upgrade Required"
#define HTTP_ERR_428_PRECONDITION_REQUIRED "428 Precondition Required"
#define HTTP_ERR_429_TOO_MANY_REQUESTS "429 Too Many Requests"
#define HTTP_ERR_431_REQUEST_HEADER_FIELDS_TOO_LARGE                           \
  "431 Request Header Fields Too Large"
#define HTTP_ERR_451_UNAVAILABLE_FOR_LEGAL_REASONS                             \
  "451 Unavailable For Legal Reasons"
#define HTTP_ERR_499_CLIENT_CLOSED_REQUEST "499 Client Closed Request"

#define HTTP_ERR_500_INTERNAL_SERVER_ERROR "500 Internal Server Error"
#define HTTP_ERR_501_NOT_IMPLEMENTED "501 Not Implemented"
#define HTTP_ERR_502_BAD_GATEWAY "502 Bad Gateway"
#define HTTP_ERR_503_SERVICE_UNAVAILABLE "503 Service Unavailable"
#define HTTP_ERR_504_GATEWAY_TIMEOUT "504 Gateway Timeout"
#define HTTP_ERR_505_HTTP_VERSION_NOT_SUPPORTED "505 HTTP Version Not Supported"
#define HTTP_ERR_506_VARIANT_ALSO_NEGOTIATES "506 Variant Also Negotiates"
#define HTTP_ERR_507_INSUFFICIENT_STORAGE "507 Insufficient Storage"
#define HTTP_ERR_508_LOOP_DETECTED "508 Loop Detected"
#define HTTP_ERR_510_NOT_EXTENDED "510 Not Extended"
#define HTTP_ERR_511_NETWORK_AUTHENTICATION_REQUIRED                           \
  "511 Network Authentication Required"
#define HTTP_ERR_599_NETWORK_CONNECT_TIMEOUT_ERR_OR                            \
  "599 Network Connect Timeout ERR_or"

// converts all httpd_err_code_t to a string representation
inline const char *convert_httpd_err_code_to_string(httpd_err_code_t err_code) {
  switch (err_code) {
  case HTTPD_500_INTERNAL_SERVER_ERROR:
    return HTTP_ERR_500_INTERNAL_SERVER_ERROR;

  case HTTPD_501_METHOD_NOT_IMPLEMENTED:
    return HTTP_ERR_501_NOT_IMPLEMENTED;

  case HTTPD_505_VERSION_NOT_SUPPORTED:
    return HTTP_ERR_505_HTTP_VERSION_NOT_SUPPORTED;

  case HTTPD_400_BAD_REQUEST:
    return HTTP_ERR_400_BAD_REQUEST;

  case HTTPD_401_UNAUTHORIZED:
    return HTTP_ERR_401_UNAUTHORIZED;

  case HTTPD_403_FORBIDDEN:
    return HTTP_ERR_403_FORBIDDEN;

  case HTTPD_404_NOT_FOUND:
    return HTTP_ERR_404_NOT_FOUND;

  case HTTPD_405_METHOD_NOT_ALLOWED:
    return HTTP_ERR_405_METHOD_NOT_ALLOWED;

  case HTTPD_408_REQ_TIMEOUT:
    return HTTP_ERR_408_REQUEST_TIMEOUT;

  case HTTPD_411_LENGTH_REQUIRED:
    return HTTP_ERR_411_LENGTH_REQUIRED;

  case HTTPD_413_CONTENT_TOO_LARGE:
    return HTTP_ERR_413_PAYLOAD_TOO_LARGE;

  case HTTPD_414_URI_TOO_LONG:
    return HTTP_ERR_414_REQUEST_URI_TOO_LONG;

  case HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE:
    return HTTP_ERR_431_REQUEST_HEADER_FIELDS_TOO_LARGE;

  default:
    return "Unknown HTTP error code";
  }
}
