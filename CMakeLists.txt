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
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(medusa_firmware C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(medusa_firmware
    src/main.cpp
    src/ssd1322.cpp
    src/framebuffer.cpp
    src/graphics.cpp
)

pico_set_program_name(medusa_firmware "medusa_firmware")
pico_set_program_version(medusa_firmware "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(medusa_firmware 0)
pico_enable_stdio_usb(medusa_firmware 1)

# Add the standard library to the build
target_link_libraries(medusa_firmware
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(medusa_firmware PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# Add any user requested libraries
target_link_libraries(medusa_firmware 
        hardware_spi
        hardware_dma
        )

pico_add_extra_outputs(medusa_firmware)

add_executable(medusa_framebuffer_test
    src/framebuffer_test_main.cpp
    src/framebuffer.cpp
)

pico_enable_stdio_uart(medusa_framebuffer_test 0)
pico_enable_stdio_usb(medusa_framebuffer_test 1)

target_link_libraries(medusa_framebuffer_test
        pico_stdlib)

target_include_directories(medusa_framebuffer_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(medusa_framebuffer_test)

add_executable(medusa_graphics_test
    src/graphics_test_main.cpp
    src/graphics.cpp
    src/framebuffer.cpp
)

pico_enable_stdio_uart(medusa_graphics_test 0)
pico_enable_stdio_usb(medusa_graphics_test 1)

target_link_libraries(medusa_graphics_test
        pico_stdlib)

target_include_directories(medusa_graphics_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(medusa_graphics_test)

add_executable(medusa_font8x8_test
    src/font8x8_test_main.cpp
    src/graphics.cpp
    src/framebuffer.cpp
)

pico_enable_stdio_uart(medusa_font8x8_test 0)
pico_enable_stdio_usb(medusa_font8x8_test 1)

target_link_libraries(medusa_font8x8_test
        pico_stdlib)

target_include_directories(medusa_font8x8_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(medusa_font8x8_test)

add_executable(medusa_renderer_test
    src/renderer_test_main.cpp
    src/graphics.cpp
    src/framebuffer.cpp
)

pico_enable_stdio_uart(medusa_renderer_test 0)
pico_enable_stdio_usb(medusa_renderer_test 1)

target_link_libraries(medusa_renderer_test
        pico_stdlib)

target_include_directories(medusa_renderer_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(medusa_renderer_test)

add_executable(medusa_renderer_oled_test
    src/renderer_oled_test_main.cpp
    src/ssd1322.cpp
    src/framebuffer.cpp
    src/graphics.cpp
)

pico_enable_stdio_uart(medusa_renderer_oled_test 0)
pico_enable_stdio_usb(medusa_renderer_oled_test 1)

target_link_libraries(medusa_renderer_oled_test
        pico_stdlib
        hardware_spi
        hardware_dma)

target_include_directories(medusa_renderer_oled_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(medusa_renderer_oled_test)

add_executable(medusa_display_button_test
    src/display_button_test_main.cpp
    src/ssd1322.cpp
    src/framebuffer.cpp
    src/graphics.cpp
)

pico_enable_stdio_uart(medusa_display_button_test 0)
pico_enable_stdio_usb(medusa_display_button_test 1)

target_link_libraries(medusa_display_button_test
        pico_stdlib
        hardware_spi
        hardware_dma)

target_include_directories(medusa_display_button_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(medusa_display_button_test)

add_executable(medusa_sprite_test
    src/sprite_test_main.cpp
    src/graphics.cpp
    src/framebuffer.cpp
)

pico_enable_stdio_uart(medusa_sprite_test 0)
pico_enable_stdio_usb(medusa_sprite_test 1)

target_link_libraries(medusa_sprite_test
        pico_stdlib)

target_include_directories(medusa_sprite_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(medusa_sprite_test)

add_executable(medusa_sprite_oled_test
    src/sprite_oled_test_main.cpp
    src/ssd1322.cpp
    src/framebuffer.cpp
    src/graphics.cpp
)

pico_enable_stdio_uart(medusa_sprite_oled_test 0)
pico_enable_stdio_usb(medusa_sprite_oled_test 1)

target_link_libraries(medusa_sprite_oled_test
        pico_stdlib
        hardware_spi
        hardware_dma)

target_include_directories(medusa_sprite_oled_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_features(medusa_sprite_oled_test PRIVATE
        cxx_std_17)

pico_add_extra_outputs(medusa_sprite_oled_test)

add_executable(medusa_settings_ui_input_test
    src/settings_ui_input_test_main.cpp
    src/ssd1322.cpp
    src/framebuffer.cpp
    src/graphics.cpp
)

pico_enable_stdio_uart(medusa_settings_ui_input_test 0)
pico_enable_stdio_usb(medusa_settings_ui_input_test 1)

target_link_libraries(medusa_settings_ui_input_test
        pico_stdlib
        hardware_spi
        hardware_dma)

target_include_directories(medusa_settings_ui_input_test PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_features(medusa_settings_ui_input_test PRIVATE
        cxx_std_17)

pico_add_extra_outputs(medusa_settings_ui_input_test)

