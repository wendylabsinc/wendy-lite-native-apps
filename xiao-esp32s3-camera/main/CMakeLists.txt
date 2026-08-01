idf_component_register(SRCS "main.c" "camera.c" "cam_capture.c" "face_cam.cpp"
                         "qr_scan.c" "web_server.c" "rom_print.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "index.html"
                    REQUIRES wendy_core
                    PRIV_REQUIRES esp_driver_gpio esp_event esp_http_server esp_netif
                                  espressif__esp32-camera espressif__esp-dl
                                  espressif__human_face_detect espressif__quirc)
