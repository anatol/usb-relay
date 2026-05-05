#include "relay.h"

#include <stddef.h>
#include <string.h>

#include "bsp/board_api.h"
#include "port_common.h"

static uint32_t relay_mask;
static uint32_t uptime_ms;
static uint32_t pulse_deadline[RELAY_MAX_COUNT];

static uint8_t relay_count(void) {
  const relay_port_cfg_t *cfg = relay_port_cfg();
  // Clamp bad board config at runtime instead of indexing past arrays.
  if (cfg->relay_count > RELAY_MAX_COUNT) return RELAY_MAX_COUNT;
  return cfg->relay_count;
}

static void write_pin(uint8_t idx, bool on) {
  const relay_port_cfg_t *cfg = relay_port_cfg();
  // Convert logical ON/OFF to electrical level per-board polarity.
  bool active = cfg->relay_active_high[idx] ? on : !on;
  relay_port_write_pin(cfg->relay_pin[idx], active);
}

void relay_init(void) {
  const relay_port_cfg_t *cfg = relay_port_cfg();
  uint8_t count = relay_count();
  relay_mask = 0;
  uptime_ms = 0;
  memset(pulse_deadline, 0, sizeof(pulse_deadline));

  relay_port_clock_enable();
  for (uint8_t i = 0; i < count; i++) {
    relay_port_pin_output(cfg->relay_pin[i]);
    write_pin(i, false);
  }
}

bool relay_valid_index(uint8_t relay) {
  uint8_t count = relay_count();
  return relay >= 1u && relay <= count;
}

void relay_set(uint8_t relay, bool on) {
  if (!relay_valid_index(relay)) return;
  uint8_t idx = relay - 1u;
  write_pin(idx, on);
  // Cache logical state so reads and toggles avoid GPIO readback quirks.
  if (on) relay_mask |= (1u << idx);
  else relay_mask &= ~(1u << idx);
}

void relay_toggle(uint8_t relay) {
  if (!relay_valid_index(relay)) return;
  uint8_t idx = relay - 1u;
  relay_set(relay, (relay_mask & (1u << idx)) == 0u);
}

void relay_set_mask(uint32_t mask) {
  uint8_t count = relay_count();
  for (uint8_t i = 0; i < count; i++) {
    relay_set(i + 1u, ((mask >> i) & 1u) != 0u);
  }
}

void relay_all_off(void) {
  relay_set_mask(0u);
}

uint32_t relay_get_mask(void) {
  return relay_mask;
}

void relay_pulse_start(uint8_t relay, uint32_t duration_ms, uint32_t now_ms) {
  if (!relay_valid_index(relay)) return;
  uint8_t idx = relay - 1u;
  relay_set(relay, true);
  // Absolute deadline keeps pulse logic simple in the poll path.
  pulse_deadline[idx] = now_ms + duration_ms;
}

void relay_pulse_poll(uint32_t now_ms) {
  uint8_t count = relay_count();
  for (uint8_t i = 0; i < count; i++) {
    uint32_t dl = pulse_deadline[i];
    // Signed-delta check keeps comparisons valid across uint32 wrap.
    if (dl != 0u && (int32_t)(now_ms - dl) >= 0) {
      pulse_deadline[i] = 0u;
      relay_set(i + 1u, false);
    }
  }
}

void relay_tick_1ms(void) {
  uptime_ms++;
}

uint32_t relay_uptime_ms(void) {
  return uptime_ms;
}

void relay_serial_hex(char *out, uint32_t out_len) {
  static const char hex[] = "0123456789ABCDEF";
  const relay_port_cfg_t *cfg = relay_port_cfg();
  // STM32 unique ID is 96 bits (12 bytes), exported as uppercase hex.
  const uint8_t *uid = (const uint8_t *)cfg->uid_base;
  uint32_t n = 12u;
  if (out_len < (n * 2u + 1u)) return;
  for (uint32_t i = 0; i < n; i++) {
    out[(i * 2u)] = hex[(uid[i] >> 4u) & 0xFu];
    out[(i * 2u) + 1u] = hex[uid[i] & 0xFu];
  }
  out[n * 2u] = '\0';
}

typedef void (*entry_fn_t)(void);

static inline void irq_disable(void) {
  __asm volatile ("cpsid i" : : : "memory");
}

static inline void set_msp(uint32_t sp) {
  __asm volatile ("msr msp, %0" : : "r" (sp));
}

void relay_enter_dfu(void) {
  const relay_port_cfg_t *cfg = relay_port_cfg();
  // Jump directly to system-memory DFU ROM vector table.
  irq_disable();
  // ROM image starts with initial MSP then reset handler entry.
  uint32_t stack = *(volatile uint32_t *)cfg->dfu_rom_addr;
  uint32_t entry = *(volatile uint32_t *)(cfg->dfu_rom_addr + 4u);
  set_msp(stack);
  ((entry_fn_t)entry)();
  while (1) {
  }
}
