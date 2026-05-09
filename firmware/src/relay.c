#include "relay.h"

#include <stddef.h>
#include <string.h>

#include "bsp/board_api.h"
#include "port_common.h"

#if defined(BOARD_FAMILY_RP2040)
#include "pico/bootrom.h"
#elif defined(BOARD_FAMILY_STM32F0)
#include "stm32f0xx.h"
#elif defined(BOARD_FAMILY_STM32F1)
#include "stm32f1xx.h"
#elif defined(BOARD_FAMILY_STM32F4)
#include "stm32f4xx.h"
#endif

static uint32_t relay_mask;
static uint32_t uptime_ms;
static uint32_t pulse_deadline[RELAY_MAX_COUNT];
static uint32_t pulse_restore_mask;

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
  pulse_restore_mask = 0;
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

uint8_t relay_supported_count(void) {
  return relay_count();
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
  bool restore_state = (relay_mask & (1u << idx)) != 0u;
  if (restore_state) pulse_restore_mask |= (1u << idx);
  else pulse_restore_mask &= ~(1u << idx);
  relay_toggle(relay);
  // Absolute deadline keeps pulse logic simple in the poll path.
  pulse_deadline[idx] = now_ms + duration_ms;
}

void relay_pulse_forced(uint8_t relay, bool start_on, uint32_t duration_ms, uint32_t now_ms) {
  if (!relay_valid_index(relay)) return;
  uint8_t idx = relay - 1u;
  if (start_on) pulse_restore_mask &= ~(1u << idx);
  else pulse_restore_mask |= (1u << idx);
  relay_set(relay, start_on);
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
      relay_set(i + 1u, (pulse_restore_mask & (1u << i)) != 0u);
      pulse_restore_mask &= ~(1u << i);
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

static const char *dfu_unavailable_reason = "DFU reboot is not supported on this board";

static bool is_sram_addr(uint32_t addr) {
  return (addr & 0xFF000000u) == 0x20000000u;
}

static bool is_thumb_code_addr(uint32_t addr) {
  return (addr & 1u) != 0u;
}

bool relay_dfu_reboot_available(void) {
  const relay_port_cfg_t *cfg = relay_port_cfg();

#if defined(BOARD_FAMILY_RP2040)
  return true;
#else
  if (cfg->dfu_rom_addr == 0u) {
    dfu_unavailable_reason = "DFU ROM address is not configured for this board";
    return false;
  }

  uint32_t stack = *(volatile uint32_t *)cfg->dfu_rom_addr;
  uint32_t entry = *(volatile uint32_t *)(cfg->dfu_rom_addr + 4u);
  if (stack == 0u || stack == 0xFFFFFFFFu || !is_sram_addr(stack)) {
    dfu_unavailable_reason = "DFU ROM vector table is invalid (stack pointer)";
    return false;
  }
  if (entry == 0u || entry == 0xFFFFFFFFu || !is_thumb_code_addr(entry)) {
    dfu_unavailable_reason = "DFU ROM vector table is invalid (entry point)";
    return false;
  }
  return true;
#endif
}

const char *relay_dfu_reboot_unavailable_reason(void) {
  return dfu_unavailable_reason;
}

typedef void (*entry_fn_t)(void);

static inline void irq_disable(void) {
  __asm volatile ("cpsid i" : : : "memory");
}

static inline void set_msp(uint32_t sp) {
  __asm volatile ("msr msp, %0" : : "r" (sp));
}

static void prepare_stm32_rom_boot(uint32_t rom_base) {
  // Stop SysTick and clear NVIC state so the ROM starts from a clean context.
  SysTick->CTRL = 0u;
  SysTick->LOAD = 0u;
  SysTick->VAL = 0u;

  const uint32_t nvic_words = (uint32_t)(sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]));
  for (uint32_t i = 0; i < nvic_words; i++) {
    NVIC->ICER[i] = 0xFFFFFFFFu;
    NVIC->ICPR[i] = 0xFFFFFFFFu;
  }

#if defined(BOARD_FAMILY_STM32F1)
  // F1 ROM expects system memory at 0x00000000.
  const uint32_t mem_mode_mask = 0x3u;
  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
  AFIO->MAPR = (AFIO->MAPR & ~mem_mode_mask) | 0x1u;
#endif

#if defined(__VTOR_PRESENT) && (__VTOR_PRESENT == 1U)
  SCB->VTOR = rom_base;
#else
  (void)rom_base;
#endif
  __DSB();
  __ISB();
}

void relay_enter_dfu(void) {
  if (!relay_dfu_reboot_available()) return;
#if defined(BOARD_FAMILY_RP2040)
  reset_usb_boot(0u, 0u);
  while (1) {
  }
#else
  const relay_port_cfg_t *cfg = relay_port_cfg();
  // Jump directly to system-memory DFU ROM vector table.
  irq_disable();
  prepare_stm32_rom_boot(cfg->dfu_rom_addr);
  // ROM image starts with initial MSP then reset handler entry.
  uint32_t stack = *(volatile uint32_t *)cfg->dfu_rom_addr;
  uint32_t entry = *(volatile uint32_t *)(cfg->dfu_rom_addr + 4u);
  set_msp(stack);
  ((entry_fn_t)entry)();
  while (1) {
  }
#endif
}
