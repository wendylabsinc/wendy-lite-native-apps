idf_component_register(SRCS "main.c" "camera.c" "cam_capture.c" "face_cam.cpp"
                         "mic.c" "qr_scan.c" "server.c" "web_server.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "index.html"
                    REQUIRES wendy_core
                    PRIV_REQUIRES esp_driver_gpio esp_driver_i2s esp_event esp_http_server esp_netif
                                  esp_timer lwip
                                  espressif__esp32-camera espressif__esp-dl
                                  espressif__human_face_detect espressif__mdns espressif__quirc)
