idf_component_register(SRCS "main.c" "camera.c" "cam_capture.c" "cam_loop.c"
                         "mic.c" "server.c"
                    INCLUDE_DIRS "."
                    REQUIRES wendy_core
                    PRIV_REQUIRES esp_driver_gpio esp_driver_i2s
                                  esp_timer lwip
                                  espressif__esp32-camera espressif__mdns)
