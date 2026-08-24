idf_component_register(SRCS "main.cpp" "cockpit.c" "ctrl.c"
                    "alt_kalman.cpp" "button.cpp" "buzzer.cpp" "flight_control.cpp" "imu.cpp"
                    "led.cpp" "pid.cpp" "rc.cpp" "sensor.cpp" "telemetry.cpp" "tof.cpp"
                    "usbserial_compat.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES abc wendy_core FastLED vl53l3c bmi270 MadgwickAHRS INA3221 OneButton
                    PRIV_REQUIRES esp_driver_gpio esp_timer)

# bmi270's common.h must be found before mbedtls' internal common.h (mbedtls
# publicly exposes its library/ dir, which otherwise comes first on -I)
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../components/bmi270)


