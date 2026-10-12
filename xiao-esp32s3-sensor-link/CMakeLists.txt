# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Pull wendy-lite's components in from its checkout on disk instead of
# vendoring copies here. Points at the local clone of
# github.com:wendylabsinc/wendy-lite, a sibling checkout
# of this project.
#set(EXTRA_COMPONENT_DIRS
#    "${CMAKE_CURRENT_LIST_DIR}/../wendy-lite/components"
#)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Only build main and its dependencies. Including every local SDK component
# enables WASM Kconfig options even when native firmware disables WASM support.
idf_build_set_property(MINIMAL_BUILD ON)

if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR
        "This project requires the XIAO ESP32S3 Sense (esp32s3), but IDF_TARGET is '${IDF_TARGET}'. "
        "Run 'idf.py set-target esp32s3' before building.")
endif()

project(xiao-esp32s3-sensor-link)
