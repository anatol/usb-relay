#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "relay.h"

typedef struct {
  volatile void *gpio_port;
  uint8_t relay_count;
  uint16_t relay_pin[RELAY_MAX_COUNT];
  bool relay_active_high[RELAY_MAX_COUNT];
  uint32_t uid_base;
  uint32_t dfu_rom_addr;
} relay_port_cfg_t;

const relay_port_cfg_t *relay_port_cfg(void);
void relay_port_clock_enable(void);
void relay_port_pin_output(uint16_t pin);
void relay_port_write_pin(uint16_t pin, bool high);
