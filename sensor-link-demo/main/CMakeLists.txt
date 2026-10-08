idf_component_register(SRCS "main.c" "rgb_led.c" "frames.c" "cam_capture.c" "cam_loop.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "frames/frame_0.jpg"
                                "frames/frame_1.jpg"
                                "frames/frame_2.jpg"
                                "frames/frame_3.jpg"
                    REQUIRES wendy_core led_strip
                    PRIV_REQUIRES esp_timer)
