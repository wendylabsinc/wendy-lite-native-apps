# INA3221 current/voltage monitor (Arduino lib, uses Wire)
idf_component_register(SRCS "src/INA3221.cpp"
                       INCLUDE_DIRS "src"
                       REQUIRES arduino-esp32)

# Third-party code; don't let IDF's -Werror promotion break the build
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-switch -Wno-maybe-uninitialized)
