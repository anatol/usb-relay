#include "port_common.h"

#include "hardware/gpio.h"
#include "pico/unique_id.h"

static uint8_t uid_bytes[12];
static bool uid_ready;

static relay_port_cfg_t cfg = {
  .gpio_port = 0,
  .relay_count = 8,
  .relay_pin = {2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u},
  .relay_active_high = {true, true, true, true, true, true, true, true},
  .uid_base = 0,
  .dfu_rom_addr = 0u,
};

const relay_port_cfg_t *relay_port_cfg(void) {
  if (!uid_ready) {
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    for (uint8_t i = 0; i < 8u; i++) uid_bytes[i] = id.id[i];
    for (uint8_t i = 8u; i < 12u; i++) uid_bytes[i] = 0u;
    cfg.uid_base = (uint32_t)(uintptr_t)uid_bytes;
    uid_ready = true;
  }
  return &cfg;
}

void relay_port_clock_enable(void) {
}

void relay_port_pin_output(uint16_t pin) {
  gpio_init((uint)pin);
  gpio_set_dir((uint)pin, GPIO_OUT);
}

void relay_port_write_pin(uint16_t pin, bool high) {
  gpio_put((uint)pin, high ? 1u : 0u);
}
