cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(zx_copro C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(PICO_COPY_TO_RAM 1)

add_executable(zx_copro
zx_copro.c
rom_emulation.c
zx_mirror.c
z80_test_image.c
)

target_link_libraries(zx_copro
		      pico_stdlib
		      hardware_gpio
			    pico_multicore
)

pico_enable_stdio_usb(zx_copro 0)
pico_enable_stdio_uart(zx_copro 0)

pico_add_extra_outputs(zx_copro)

