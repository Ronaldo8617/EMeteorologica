# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(EMeteorologica C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
include_directories( ${CMAKE_SOURCE_DIR}/lib ) 
add_executable(EMeteorologica EMeteorologica.c 
        lib/aht20.c 
        lib/bmp280.c 
        lib/ssd1306.c
        )

pico_set_program_name(EMeteorologica "EMeteorologica")
pico_set_program_version(EMeteorologica "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(EMeteorologica 0)
pico_enable_stdio_usb(EMeteorologica 1)

# Add the standard library to the build
target_link_libraries(EMeteorologica
        pico_stdlib
        hardware_i2c
        hardware_pio
        hardware_pwm
        hardware_adc       
        pico_cyw43_arch_lwip_threadsafe_background)

# Add the standard include files to the build
target_include_directories(EMeteorologica PRIVATE
        ${CMAKE_CURRENT_LIST_DIR} 
)




pico_add_extra_outputs(EMeteorologica)

