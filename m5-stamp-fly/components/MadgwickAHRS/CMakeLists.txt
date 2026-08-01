# Madgwick AHRS filter, vendored from M5StampFly lib/MdgwickAHRS
idf_component_register(SRCS "MadgwickAHRS.cpp"
                       INCLUDE_DIRS ".")

# Third-party code; don't let IDF's -Werror promotion break the build
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-uninitialized)
