#include "web/server.hpp"

#include <cstdlib>
#include <string>
#include <vector>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "control/commands.hpp"
#include "monitor/prober.hpp"
#include "ota/ble.hpp"
#include "ota/engine.hpp"
#include "web/assets.hpp"

namespace web {
namespace {

constexpr const char *TAG = "WEB";
constexpr size_t kMaxConfigBody = 8192;
constexpr size_t kMaxOtaChunk = 16 * 1024;

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

esp_err_t post_control(httpd_req_t *req) {
  if (req->content_len == 0 || req->content_len > 1024) {
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

  cJSON *root = cJSON_Parse(body.c_str());
  if (root == nullptr) {
    return send_message(req, false, "malformed json", "400 Bad Request");
  }

  const cJSON *action = cJSON_GetObjectItemCaseSensitive(root, "action");
  if (!cJSON_IsString(action)) {
    cJSON_Delete(root);
    return send_message(req, false, "action is required", "400 Bad Request");
  }

  const std::string name = action->valuestring;
  control::Command command;

  if (name == "refresh") {
    command.action = control::Action::Refresh;
  } else if (name == "probe") {
    command.action = control::Action::ProbeNow;
  } else if (name == "reboot") {
    command.action = control::Action::Reboot;
  } else if (name == "page") {
    const cJSON *page = cJSON_GetObjectItemCaseSensitive(root, "page");
    if (!cJSON_IsString(page)) {
      cJSON_Delete(root);
      return send_message(req, false, "page is required", "400 Bad Request");
    }
    const std::string target = page->valuestring;
    if (target != "monitor" && target != "status") {
      cJSON_Delete(root);
      return send_message(req, false, "unknown page", "400 Bad Request");
    }
    command.action = control::Action::ShowPage;
    command.page = target;
  } else {
    cJSON_Delete(root);
    return send_message(req, false, "unknown action", "400 Bad Request");
  }

  cJSON_Delete(root);
  control::queue().post(command);
  ESP_LOGI(TAG, "control: %s%s%s", name.c_str(),
           command.action == control::Action::ShowPage ? " " : "",
           command.action == control::Action::ShowPage ? command.page.c_str()
                                                       : "");
  return send_message(req, true, "", nullptr);
}

bool read_body(httpd_req_t *req, size_t limit, std::vector<uint8_t> &out,
               std::string &error) {
  out.clear();
  if (req->content_len > 0) {
    if (static_cast<size_t>(req->content_len) > limit) {
      error = "body is too large";
      return false;
    }
    out.resize(static_cast<size_t>(req->content_len));
    size_t received = 0;
    while (received < out.size()) {
      const int read =
          httpd_req_recv(req, reinterpret_cast<char *>(out.data()) + received,
                         out.size() - received);
      if (read <= 0) {
        error = "could not read body";
        return false;
      }
      received += static_cast<size_t>(read);
    }
    return true;
  }

  // Chunked transfer encoding: the HTTP server decodes it for us.
  uint8_t buffer[1024];
  for (;;) {
    const int read =
        httpd_req_recv(req, reinterpret_cast<char *>(buffer), sizeof(buffer));
    if (read == 0) {
      break;
    }
    if (read < 0) {
      error = "could not read chunked body";
      return false;
    }
    if (out.size() + static_cast<size_t>(read) > limit) {
      error = "body is too large";
      return false;
    }
    out.insert(out.end(), buffer, buffer + read);
  }
  if (out.empty()) {
    error = "body is empty";
    return false;
  }
  return true;
}

esp_err_t get_ota(httpd_req_t *req) {
  const ota::Info info = ota::info();
  const ota::ble::Info ble = ota::ble::info();

  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "available", info.available);
  cJSON_AddStringToObject(root, "state", ota::state_name(info.state));
  cJSON_AddNumberToObject(root, "offset", info.offset);
  cJSON_AddNumberToObject(root, "total", info.total);
  cJSON_AddNumberToObject(root, "capacity", info.capacity);
  cJSON_AddNumberToObject(root, "chunk_size", kMaxOtaChunk);
  cJSON_AddStringToObject(root, "partition", info.partition);
  cJSON_AddStringToObject(root, "running_partition", info.running);
  cJSON_AddStringToObject(root, "sha256", info.sha256);
  cJSON_AddStringToObject(root, "error", info.error);

  cJSON *ble_json = cJSON_AddObjectToObject(root, "ble");
  cJSON_AddBoolToObject(ble_json, "enabled", ble.enabled);
  cJSON_AddBoolToObject(ble_json, "connected", ble.connected);
  cJSON_AddNumberToObject(ble_json, "mtu", ble.mtu);
  cJSON_AddStringToObject(ble_json, "name", ble.name);
  cJSON_AddStringToObject(ble_json, "service_uuid", ble.service_uuid);
  cJSON_AddStringToObject(ble_json, "control_uuid", ble.control_uuid);
  cJSON_AddStringToObject(ble_json, "data_uuid", ble.data_uuid);
  cJSON_AddStringToObject(ble_json, "status_uuid", ble.status_uuid);

  return send_json(req, root);
}

esp_err_t post_ota_begin(httpd_req_t *req) {
  std::vector<uint8_t> body;
  std::string error;
  if (!read_body(req, 1024, body, error)) {
    return send_message(req, false, error, "400 Bad Request");
  }

  cJSON *root = cJSON_ParseWithLength(
      reinterpret_cast<const char *>(body.data()), body.size());
  if (root == nullptr) {
    return send_message(req, false, "malformed json", "400 Bad Request");
  }

  const cJSON *size = cJSON_GetObjectItemCaseSensitive(root, "size");
  const cJSON *sha = cJSON_GetObjectItemCaseSensitive(root, "sha256");
  const cJSON *restart = cJSON_GetObjectItemCaseSensitive(root, "restart");

  if (!cJSON_IsNumber(size) || size->valuedouble < 0 ||
      size->valuedouble > 0xffffffffu || !cJSON_IsString(sha)) {
    cJSON_Delete(root);
    return send_message(req, false, "size and sha256 are required",
                        "400 Bad Request");
  }

  uint32_t offset = 0;
  const bool ok =
      ota::begin(static_cast<uint32_t>(size->valuedouble), sha->valuestring,
                 cJSON_IsBool(restart) && cJSON_IsTrue(restart), offset, error);
  cJSON_Delete(root);

  if (!ok) {
    return send_message(req, false, error, "400 Bad Request");
  }

  cJSON *answer = cJSON_CreateObject();
  cJSON_AddBoolToObject(answer, "ok", true);
  cJSON_AddNumberToObject(answer, "offset", offset);
  return send_json(req, answer);
}

esp_err_t post_ota_chunk(httpd_req_t *req) {
  char header[16];
  if (httpd_req_get_hdr_value_str(req, "X-OTA-Offset", header,
                                  sizeof(header)) != ESP_OK) {
    return send_message(req, false, "X-OTA-Offset header is required",
                        "400 Bad Request");
  }

  char *end = nullptr;
  const unsigned long offset = std::strtoul(header, &end, 10);
  if (end == header || *end != '\0') {
    return send_message(req, false, "invalid X-OTA-Offset", "400 Bad Request");
  }

  std::vector<uint8_t> body;
  std::string error;
  if (!read_body(req, kMaxOtaChunk, body, error)) {
    return send_message(req, false, error, "400 Bad Request");
  }

  if (!ota::append(static_cast<uint32_t>(offset), body.data(),
                   static_cast<uint32_t>(body.size()), error)) {
    cJSON *answer = cJSON_CreateObject();
    cJSON_AddBoolToObject(answer, "ok", false);
    cJSON_AddStringToObject(answer, "error", error.c_str());
    cJSON_AddNumberToObject(answer, "offset", ota::offset());
    return send_json(req, answer, "409 Conflict");
  }

  cJSON *answer = cJSON_CreateObject();
  cJSON_AddBoolToObject(answer, "ok", true);
  cJSON_AddNumberToObject(answer, "offset", ota::offset());
  return send_json(req, answer);
}

esp_err_t post_ota_finish(httpd_req_t *req) {
  std::string error;
  if (!ota::finish(error)) {
    cJSON *answer = cJSON_CreateObject();
    cJSON_AddBoolToObject(answer, "ok", false);
    cJSON_AddStringToObject(answer, "error", error.c_str());
    cJSON_AddNumberToObject(answer, "offset", ota::offset());
    return send_json(req, answer, "409 Conflict");
  }

  const ota::Info info = ota::info();
  cJSON *answer = cJSON_CreateObject();
  cJSON_AddBoolToObject(answer, "ok", true);
  cJSON_AddStringToObject(answer, "state", ota::state_name(info.state));
  cJSON_AddStringToObject(answer, "partition", info.partition);
  cJSON_AddNumberToObject(answer, "offset", info.offset);
  cJSON_AddNumberToObject(answer, "total", info.total);
  cJSON_AddBoolToObject(answer, "reboot_required", true);
  return send_json(req, answer);
}

esp_err_t post_ota_abort(httpd_req_t *req) {
  std::string error;
  if (!ota::abort(error)) {
    return send_message(req, false, error, "500 Internal Server Error");
  }
  return send_message(req, true, "", nullptr);
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
  config.max_uri_handlers = 16;
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
      {.uri = "/api/control", .method = HTTP_POST, .handler = post_control,
       .user_ctx = nullptr},
      {.uri = "/api/ota", .method = HTTP_GET, .handler = get_ota,
       .user_ctx = nullptr},
      {.uri = "/api/ota/begin", .method = HTTP_POST, .handler = post_ota_begin,
       .user_ctx = nullptr},
      {.uri = "/api/ota/chunk", .method = HTTP_POST, .handler = post_ota_chunk,
       .user_ctx = nullptr},
      {.uri = "/api/ota/finish", .method = HTTP_POST, .handler = post_ota_finish,
       .user_ctx = nullptr},
      {.uri = "/api/ota/abort", .method = HTTP_POST, .handler = post_ota_abort,
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
