# ST VL53L3CX (VL53LX) ToF driver, vendored from M5StampFly lib/vl53l3c.
# I2C goes through Arduino Wire1 (vl53lx_wire.cpp): the legacy driver/i2c.h
# API cannot be linked next to the new i2c_master driver Wire uses.
file(GLOB srcs "*.c" "*.cpp")
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES arduino-esp32)
