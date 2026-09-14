idf_component_register(SRCS "main.c"
                            "max6675.c"
                            "gnss.c"
                            "kiln_state.c"
                             "data_model.c"
                             "dashboard.c"
                             "ble_service.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_timer esp_wifi esp_event esp_netif esp_http_server nvs_flash driver bt)
