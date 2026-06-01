# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico_w CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(magnetic_initial C CXX ASM)

pico_sdk_init()

add_executable(magnetic_initial
    main.cpp
)

# Added hardware_adc to the list
target_link_libraries(magnetic_initial
    pico_stdlib
    hardware_pwm
    hardware_gpio
    hardware_adc
)

target_include_directories(magnetic_initial PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_enable_stdio_usb(magnetic_initial 1)
pico_enable_stdio_uart(magnetic_initial 0)

pico_add_extra_outputs(magnetic_initial)
