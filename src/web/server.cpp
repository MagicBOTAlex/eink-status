#include "web/server.hpp"

#include <string>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "monitor/prober.hpp"
#include "web/assets.hpp"

namespace web {
namespace {

constexpr const char *TAG = "WEB";
constexpr size_t kMaxConfigBody = 8192;

httpd_handle_t s_server = nullptr;

esp_err_t send_json(httpd_req_t *req, cJSON *root, const char *status = nullptr) {
  char *text = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (text == nullptr) {
    return httpd_resp_send_500(req);
  }
  if (status != nullptr) {
    httpd_resp_set_status(req, status);
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  const esp_err_t err = httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
  cJSON_free(text);
  return err;
}

esp_err_t send_message(httpd_req_t *req, bool ok, const std::string &error,
                       const char *status) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "ok", ok);
  if (!error.empty()) {
    cJSON_AddStringToObject(root, "error", error.c_str());
  }
  return send_json(req, root, status);
}

esp_err_t get_index(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, kIndexHtml, static_cast<ssize_t>(kIndexHtmlSize));
}

esp_err_t get_css(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/css; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "max-age=60");
  return httpd_resp_send(req, kAppCss, static_cast<ssize_t>(kAppCssSize));
}

esp_err_t get_config(httpd_req_t *req) {
  const std::string json = monitor::config_json();
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json.c_str(), static_cast<ssize_t>(json.size()));
}

esp_err_t get_status(httpd_req_t *req) {
  const std::string json = monitor::snapshot_json();
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json.c_str(), static_cast<ssize_t>(json.size()));
}

esp_err_t post_config(httpd_req_t *req) {
  if (req->content_len == 0 || req->content_len > kMaxConfigBody) {
    return send_message(req, false, "body is empty or too large",
                        "400 Bad Request");
  }

  std::string body(req->content_len, '\0');
  size_t received = 0;
  while (received < body.size()) {
    const int read = httpd_req_recv(req, &body[received], body.size() - received);
    if (read <= 0) {
      return send_message(req, false, "could not read body", "400 Bad Request");
    }
    received += static_cast<size_t>(read);
  }

  std::string error;
  if (!monitor::set_config_json(body, error)) {
    return send_message(req, false, error, "400 Bad Request");
  }
  return send_message(req, true, "", nullptr);
}

} // namespace

void start() {
  if (s_server != nullptr) {
    return;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 8;
  config.stack_size = 8192;
  config.lru_purge_enable = true;

  if (httpd_start(&s_server, &config) != ESP_OK) {
    ESP_LOGE(TAG, "http server failed to start");
    s_server = nullptr;
    return;
  }

  const httpd_uri_t routes[] = {
      {.uri = "/", .method = HTTP_GET, .handler = get_index, .user_ctx = nullptr},
      {.uri = "/app.css", .method = HTTP_GET, .handler = get_css,
       .user_ctx = nullptr},
      {.uri = "/api/config", .method = HTTP_GET, .handler = get_config,
       .user_ctx = nullptr},
      {.uri = "/api/config", .method = HTTP_POST, .handler = post_config,
       .user_ctx = nullptr},
      {.uri = "/api/status", .method = HTTP_GET, .handler = get_status,
       .user_ctx = nullptr},
  };

  for (const httpd_uri_t &route : routes) {
    if (httpd_register_uri_handler(s_server, &route) != ESP_OK) {
      ESP_LOGW(TAG, "could not register %s", route.uri);
    }
  }
  ESP_LOGI(TAG, "web UI on http://<device-ip>/ (%u B css, %u B html)",
           (unsigned)kAppCssSize, (unsigned)kIndexHtmlSize);
}

} // namespace web
