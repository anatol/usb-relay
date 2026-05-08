#include "port_common.h"

#include "stm32f0xx.h"

static const relay_port_cfg_t cfg = {
  .gpio_port = GPIOA,
  .relay_count = 8,
  .relay_pin = {
    (1u << 0), (1u << 1), (1u << 2), (1u << 3),
    (1u << 4), (1u << 5), (1u << 6), (1u << 7),
  },
  .relay_active_high = {true, true, true, true, true, true, true, true},
  .uid_base = 0x1FFFF7ACu,
  .dfu_rom_addr = 0x1FFFC800u,
};

const relay_port_cfg_t *relay_port_cfg(void) { return &cfg; }

void relay_port_clock_enable(void) { RCC->AHBENR |= RCC_AHBENR_GPIOAEN; }

void relay_port_pin_output(uint16_t pin) {
  volatile GPIO_TypeDef *port = (volatile GPIO_TypeDef *)cfg.gpio_port;
  uint8_t bit = 0;
  // pin is a single-bit mask, convert to index for MODER fields
  while (((pin >> bit) & 1u) == 0u) bit++;
  port->MODER &= ~(0x3u << (bit * 2u));
  port->MODER |= (0x1u << (bit * 2u));
}

void relay_port_write_pin(uint16_t pin, bool high) {
  volatile GPIO_TypeDef *port = (volatile GPIO_TypeDef *)cfg.gpio_port;
  // BSRR lower half sets bits, upper half clears bits atomically.
  if (high) port->BSRR = pin;
  else port->BSRR = ((uint32_t)pin << 16u);
}
