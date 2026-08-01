# Bosch BMI270 IMU driver, vendored from M5StampFly lib/bmi270
idf_component_register(SRCS "bmi2.c" "bmi270.c" "bmi270_maximum_fifo.c" "common.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver)
