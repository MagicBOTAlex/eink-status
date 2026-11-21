idf_component_register(
    SRCS "main.cpp" "epd.cpp" "wifi.cpp"
         "fonts/cozette_vector_16.c" "fonts/cozette_vector_48.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver lvgl esp_wifi nvs_flash esp_event esp_netif
)
