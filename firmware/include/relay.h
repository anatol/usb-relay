#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RELAY_MAX_COUNT 8

void relay_init(void);
void relay_set(uint8_t relay, bool on);
void relay_toggle(uint8_t relay);
void relay_set_mask(uint32_t mask);
void relay_all_off(void);
uint32_t relay_get_mask(void);
bool relay_valid_index(uint8_t relay);
uint8_t relay_supported_count(void);

void relay_pulse_start(uint8_t relay, uint32_t duration_ms, uint32_t now_ms);
void relay_pulse_forced(uint8_t relay, bool start_on, uint32_t duration_ms, uint32_t now_ms);
void relay_pulse_poll(uint32_t now_ms);

uint32_t relay_uptime_ms(void);
void relay_tick_1ms(void);

void relay_serial_hex(char *out, uint32_t out_len);
bool relay_dfu_reboot_available(void);
const char *relay_dfu_reboot_unavailable_reason(void);
void relay_enter_dfu(void);
