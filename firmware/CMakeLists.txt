cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED BOARD_FAMILY)
  message(FATAL_ERROR "Set -DBOARD_FAMILY={STM32F0|STM32F1|STM32F4|RP2040}")
endif()

# Locate TinyUSB checkout (preferred next to firmware/, fallback to vendored firmware/tinyusb)
set(TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/../tinyusb)
if(NOT EXISTS ${TINYUSB_PATH}/hw/bsp/family_support.cmake)
  set(TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/tinyusb)
endif()
if(NOT EXISTS ${TINYUSB_PATH}/hw/bsp/family_support.cmake)
  message(FATAL_ERROR "TinyUSB is missing at ${TINYUSB_PATH}. Clone it next to firmware/")
endif()

# Map project board families to TinyUSB FAMILY/BOARD defaults.
# Users can still override with -DFAMILY=... or -DBOARD=... directly.
if(NOT DEFINED FAMILY)
  if(BOARD_FAMILY STREQUAL "STM32F0")
    set(FAMILY stm32f0 CACHE STRING "TinyUSB family")
  elseif(BOARD_FAMILY STREQUAL "STM32F1")
    set(FAMILY stm32f1 CACHE STRING "TinyUSB family")
  elseif(BOARD_FAMILY STREQUAL "STM32F4")
    set(FAMILY stm32f4 CACHE STRING "TinyUSB family")
  elseif(BOARD_FAMILY STREQUAL "RP2040")
    set(FAMILY rp2040 CACHE STRING "TinyUSB family")
  else()
    message(FATAL_ERROR "Unsupported BOARD_FAMILY=${BOARD_FAMILY}")
  endif()
endif()

if(NOT DEFINED BOARD)
  if(FAMILY STREQUAL "stm32f0")
    set(BOARD stm32f072disco CACHE STRING "TinyUSB board")
  elseif(FAMILY STREQUAL "stm32f1")
    set(BOARD stm32f103_bluepill CACHE STRING "TinyUSB board")
  elseif(FAMILY STREQUAL "stm32f4")
    set(BOARD stm32f411blackpill CACHE STRING "TinyUSB board")
  elseif(FAMILY STREQUAL "rp2040")
    # TinyUSB board defaults to Pico; users can override with:
    # -DBOARD=waveshare_rp2040_zero
    set(BOARD raspberry_pi_pico CACHE STRING "TinyUSB board")
  endif()
endif()

include(${TINYUSB_PATH}/hw/bsp/family_support.cmake)

project(usb_relay_stm32 C CXX ASM)
family_initialize_project(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR})

if (RTOS STREQUAL zephyr)
  set(EXE_NAME app)
else()
  set(EXE_NAME usb-relay)
  add_executable(${EXE_NAME})
endif()

add_compile_definitions(USB_RELAY_FW_VERSION=\"0.1.0\")
add_compile_definitions(BOARD_FAMILY_${BOARD_FAMILY}=1)

target_sources(${EXE_NAME} PRIVATE
  src/main.c
  src/relay.c
  src/command.c
  src/usb_descriptors.c
)

if(BOARD_FAMILY STREQUAL "STM32F0")
  set(MCU_VARIANT stm32f072xb)
  target_sources(${EXE_NAME} PRIVATE boards/ports/port_stm32f0.c)
  target_compile_definitions(${EXE_NAME} PRIVATE STM32F072xB)
elseif(BOARD_FAMILY STREQUAL "STM32F1")
  set(MCU_VARIANT stm32f103xb)
  target_sources(${EXE_NAME} PRIVATE boards/ports/port_stm32f1.c)
  target_compile_definitions(${EXE_NAME} PRIVATE STM32F103xB)
elseif(BOARD_FAMILY STREQUAL "STM32F4")
  set(MCU_VARIANT stm32f411xe)
  target_sources(${EXE_NAME} PRIVATE boards/ports/port_stm32f4.c)
  target_compile_definitions(${EXE_NAME} PRIVATE STM32F411xE)
elseif(BOARD_FAMILY STREQUAL "RP2040")
  target_sources(${EXE_NAME} PRIVATE boards/ports/port_rp2040.c)
else()
  message(FATAL_ERROR "Unsupported BOARD_FAMILY=${BOARD_FAMILY}")
endif()

target_include_directories(${EXE_NAME} PRIVATE
  include
  boards/ports
)

family_configure_device_example(${EXE_NAME} ${RTOS})
