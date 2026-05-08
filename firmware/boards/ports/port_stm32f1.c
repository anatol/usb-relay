#include "port_common.h"

#include "stm32f1xx.h"

static const relay_port_cfg_t cfg = {
  .gpio_port = GPIOB,
  .relay_count = 8,
  .relay_pin = {
    (1u << 12), (1u << 13), (1u << 14), (1u << 15),
    (1u << 8), (1u << 9), (1u << 10), (1u << 11),
  },
  .relay_active_high = {false, false, false, false, false, false, false, false},
  .uid_base = 0x1FFFF7E8u,
  .dfu_rom_addr = 0x1FFFF000u,
};

const relay_port_cfg_t *relay_port_cfg(void) { return &cfg; }

void relay_port_clock_enable(void) { RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; }

void relay_port_pin_output(uint16_t pin) {
  volatile GPIO_TypeDef *port = (volatile GPIO_TypeDef *)cfg.gpio_port;
  uint8_t bit = 0;
  volatile uint32_t *reg;
  uint8_t shift;
  // pin is a single-bit mask, convert to index for CRL/CRH fields
  while (((pin >> bit) & 1u) == 0u) bit++;
  if (bit < 8u) {
    reg = &port->CRL;
    shift = bit * 4u;
  } else {
    reg = &port->CRH;
    shift = (bit - 8u) * 4u;
  }
  *reg &= ~(0xFu << shift);
  *reg |= (0x2u << shift); // Output push-pull, 2 MHz
}

void relay_port_write_pin(uint16_t pin, bool high) {
  volatile GPIO_TypeDef *port = (volatile GPIO_TypeDef *)cfg.gpio_port;
  // BSRR lower half sets bits, upper half clears bits atomically.
  if (high) port->BSRR = pin;
  else port->BSRR = ((uint32_t)pin << 16u);
}
