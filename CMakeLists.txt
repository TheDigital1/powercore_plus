cmake_minimum_required(VERSION 3.13)

add_executable(powercore
	powercore.c
)


# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(powercore)

target_link_libraries(powercore pico_stdlib hardware_pwm hardware_adc hardware_dma)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

pico_enable_stdio_usb(powercore 1)
pico_enable_stdio_uart(powercore 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(powercore)
