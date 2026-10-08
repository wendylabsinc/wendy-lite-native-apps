idf_component_register(SRCS "main.c" "cam_capture.c" "cam_loop.c" "mic.c"
                    INCLUDE_DIRS "."
                    REQUIRES wendy_core
                    PRIV_REQUIRES esp_driver_gpio esp_driver_i2s
                                  esp_timer
                                  espressif__esp32-camera)
