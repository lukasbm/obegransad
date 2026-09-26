# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.10)

# Layer optional defaults files on top of the tracked sdkconfig.defaults:
#  - sdkconfig.defaults.local is gitignored and holds secrets (WiFi
#    SSID/password) plus personal config. Later files take precedence.
#  - OBG_SIM=1 additionally applies sdkconfig.defaults.sim. The simulator
#    scripts pass -DSDKCONFIG_DEFAULTS explicitly instead, so this is only a
#    convenience shortcut. Both use a dedicated build directory, so the
#    hardware sdkconfig is never touched.
if(NOT DEFINED SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.local")
        list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.local")
    endif()
    if(DEFINED ENV{OBG_SIM})
        list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.sim")
    endif()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(obegransad)
