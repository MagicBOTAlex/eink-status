set(UI_SRCS
    "ui/app.cpp"
    "ui/canvas.cpp"
    "ui/component.cpp"
    "ui/components.cpp"
    "ui/font.cpp"
    "ui/fonts/cozette_16.cpp"
    "ui/fonts/cozette_24.cpp"
    "ui/fonts/cozette_32.cpp"
    "ui/fonts/cozette_96.cpp"
    "ui/page.cpp"
    "ui/param.cpp"
    "ui/rotation.cpp"
    "control/commands.cpp"
    "ota/session.cpp"
    "ota/engine.cpp"
    "ota/ble.cpp"
    "ui/types.cpp"
    "screens/status_page.cpp"
    "screens/monitor_page.cpp"
    "monitor/model.cpp"
    "monitor/config.cpp"
    "monitor/prober.cpp"
    "web/assets.cpp"
    "web/server.cpp")

idf_component_register(
    SRCS "main.cpp" "epd.cpp" "wifi.cpp" ${UI_SRCS}
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver esp_wifi nvs_flash esp_event esp_netif
                  esp_http_client esp-tls esp_http_server json lwip esp_timer
                  app_update mbedtls bt
)
