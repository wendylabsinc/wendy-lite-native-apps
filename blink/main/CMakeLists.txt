idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES wendy_core
                    PRIV_REQUIRES esp_driver_gpio)
