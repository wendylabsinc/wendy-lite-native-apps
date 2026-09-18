idf_component_register(SRCS "main.c" "rgb_led.c" "payload.c" "tcp_server.c" "tls_server.c"
                    INCLUDE_DIRS "."
                    REQUIRES wendy_core led_strip
                    PRIV_REQUIRES lwip esp-tls mbedtls wendy_conf esp_timer)
