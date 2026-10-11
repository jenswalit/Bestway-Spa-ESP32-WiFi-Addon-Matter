cmake_minimum_required(VERSION 3.5)

if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "ESP_MATTER_PATH is not set. Run: source ~/esp/esp-matter/export.sh")
endif()

if(NOT DEFINED ENV{ESP_MATTER_DEVICE_PATH})
    if("${IDF_TARGET}" STREQUAL "esp32c6" OR "${IDF_TARGET}" STREQUAL "")
        set(ENV{ESP_MATTER_DEVICE_PATH}
            $ENV{ESP_MATTER_PATH}/device_hal/device/esp32c6_devkit_c)
    else()
        message(FATAL_ERROR "Bestway_Matter_v5 is intended for ESP32-C6")
    endif()
endif()

set(PROJECT_VER "5.0.1")
set(PROJECT_VER_NUMBER 5002)

set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include(${ESP_MATTER_PATH}/examples/common/cmake_common/components_include.cmake)
include($ENV{ESP_MATTER_DEVICE_PATH}/esp_matter_device.cmake)

set(EXTRA_COMPONENT_DIRS
    "${ESP_MATTER_PATH}/examples/common"
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components"
    "${ESP_MATTER_PATH}/device_hal/device"
    ${extra_components_dirs_append}
)

project(Bestway_Matter_v5)

idf_build_set_property(COMPILE_OPTIONS
    "-DCHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME=\"PtW spa controls\""
    "-DCHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME=\"Bestway Spa Bridge\""
    "-DCHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION=2"
    "-DCHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING=\"BWC_PCB_V2M\""
    "-DCHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION=${PROJECT_VER_NUMBER}"
    "-DCHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING=\"${PROJECT_VER}\""
    APPEND
)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)

