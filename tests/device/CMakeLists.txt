# On-device tests. Build for a target, then run on hardware or in Wokwi:
#   idf.py -B build/esp32 -D SDKCONFIG=build/esp32/sdkconfig \
#          -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32" set-target esp32 build
cmake_minimum_required(VERSION 3.16)

# The repository root is the esp32-psram-stl component.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(psram_stl_device_tests)
