#include "command.h"

#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "relay.h"
#include "bsp/board_api.h"
#include "tusb.h"

static uint32_t now_ms;
// Default pulse length when `pulse <relay>` omits duration: 1 second.
static const uint32_t kPulseDefaultDurationMs = 1000u;

void command_init(void) {
  now_ms = 0;
}

uint32_t command_now_ms(void) {
  return now_ms;
}

void command_poll(void) {
  static uint32_t last = 0;
  uint32_t ms = tusb_time_millis_api();
  // Catch up every elapsed millisecond so pulse timing stays monotonic.
  while (last != ms) {
    last++;
    now_ms++;
    relay_tick_1ms();
  }
}

void command_write_line(const char *line) {
  // Responses are always CRLF-terminated for compatibility with
  // common serial terminals and simple line readers.
  tud_cdc_write_str(line);
  tud_cdc_write_str("\r\n");
  tud_cdc_write_flush();
}

void command_writef(const char *fmt, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  command_write_line(buf);
}

static bool parse_uint(const char *s, uint32_t *v) {
  char *end = NULL;
  unsigned long parsed = strtoul(s, &end, 0);
  // Accept decimal/hex input; reject trailing junk.
  if (s == end || *end != '\0') return false;
  *v = (uint32_t)parsed;
  return true;
}

static void cmd_help(void) {
  // Keep this list machine-parseable: one tokenized capability line.
  command_write_line("OK help id version uptime state set toggle pulse pulse-on pulse-off setmask all reboot-dfu");
}

static void cmd_state(void) {
  command_writef("STATE 0x%02lX", (unsigned long)relay_get_mask());
}

static void cmd_id(void) {
  char serial[25];
  relay_serial_hex(serial, sizeof(serial));
  command_writef("ID %s", serial);
}

static void cmd_version(void) {
  command_writef("VERSION usb-relay %s", USB_RELAY_FW_VERSION);
}

static void cmd_uptime(void) {
  command_writef("UPTIME %lu", (unsigned long)relay_uptime_ms());
}

static void syntax_error(const char *cmd, const char *usage) {
  command_writef("error: %s command syntax is \"%s\"", cmd, usage);
}

static void cmd_set(char **argv, int argc) {
  uint32_t relay;
  if (argc != 3) {
    syntax_error("set", "set <relay> <on|off>");
    return;
  }
  if (!parse_uint(argv[1], &relay) || !relay_valid_index((uint8_t)relay)) {
    command_write_line("ERR invalid-relay");
    return;
  }
  if (strcmp(argv[2], "on") == 0) {
    relay_set((uint8_t)relay, true);
  } else if (strcmp(argv[2], "off") == 0) {
    relay_set((uint8_t)relay, false);
  } else {
    syntax_error("set", "set <relay> <on|off>");
    return;
  }
  command_write_line("OK");
}

static void cmd_toggle(char **argv, int argc) {
  uint32_t relay;
  if (argc != 2 || !parse_uint(argv[1], &relay)) {
    syntax_error("toggle", "toggle <relay>");
    return;
  }
  if (!relay_valid_index((uint8_t)relay)) {
    command_write_line("ERR invalid-relay");
    return;
  }
  relay_toggle((uint8_t)relay);
  command_write_line("OK");
}

static void cmd_pulse(char **argv, int argc) {
  uint32_t relay;
  uint32_t duration = kPulseDefaultDurationMs;
  if (argc != 2 && argc != 3) {
    syntax_error("pulse", "pulse <relay> [duration-ms]");
    return;
  }
  if (!parse_uint(argv[1], &relay) || !relay_valid_index((uint8_t)relay)) {
    command_write_line("ERR invalid-relay");
    return;
  }
  if (argc == 3) {
    if (!parse_uint(argv[2], &duration) || duration == 0u) {
      syntax_error("pulse", "pulse <relay> [duration-ms]");
      return;
    }
  }
  relay_pulse_start((uint8_t)relay, duration, now_ms);
  command_write_line("OK");
}

static void cmd_pulse_on(char **argv, int argc) {
  uint32_t relay;
  uint32_t duration = kPulseDefaultDurationMs;
  if (argc != 2 && argc != 3) {
    syntax_error("pulse-on", "pulse-on <relay> [duration-ms]");
    return;
  }
  if (!parse_uint(argv[1], &relay) || !relay_valid_index((uint8_t)relay)) {
    command_write_line("ERR invalid-relay");
    return;
  }
  if (argc == 3) {
    if (!parse_uint(argv[2], &duration) || duration == 0u) {
      syntax_error("pulse-on", "pulse-on <relay> [duration-ms]");
      return;
    }
  }
  relay_pulse_forced((uint8_t)relay, true, duration, now_ms);
  command_write_line("OK");
}

static void cmd_pulse_off(char **argv, int argc) {
  uint32_t relay;
  uint32_t duration = kPulseDefaultDurationMs;
  if (argc != 2 && argc != 3) {
    syntax_error("pulse-off", "pulse-off <relay> [duration-ms]");
    return;
  }
  if (!parse_uint(argv[1], &relay) || !relay_valid_index((uint8_t)relay)) {
    command_write_line("ERR invalid-relay");
    return;
  }
  if (argc == 3) {
    if (!parse_uint(argv[2], &duration) || duration == 0u) {
      syntax_error("pulse-off", "pulse-off <relay> [duration-ms]");
      return;
    }
  }
  relay_pulse_forced((uint8_t)relay, false, duration, now_ms);
  command_write_line("OK");
}

static void cmd_setmask(char **argv, int argc) {
  uint32_t mask;
  if (argc != 2 || !parse_uint(argv[1], &mask)) {
    syntax_error("setmask", "setmask <mask>");
    return;
  }
  relay_set_mask(mask);
  command_write_line("OK");
}

static void cmd_all(char **argv, int argc) {
  if (argc == 2 && strcmp(argv[1], "off") == 0) {
    relay_all_off();
    command_write_line("OK");
    return;
  }
  syntax_error("all", "all off");
}

static void cmd_reboot_dfu(void) {
  if (!relay_dfu_reboot_available()) {
    command_writef("ERR cannot enter DFU mode: %s", relay_dfu_reboot_unavailable_reason());
    return;
  }
  // ACK first so host gets a deterministic final response before reboot.
  // If this succeeded, USB will disconnect immediately after this response.
  command_write_line("OK entering DFU bootloader");
  relay_enter_dfu();
}

void command_process_line(const char *line) {
  char buf[128];
  char *argv[8];
  int argc = 0;

  strncpy(buf, line, sizeof(buf) - 1u);
  buf[sizeof(buf) - 1u] = '\0';

  // Tokenize in-place: argv[] points into buf.
  char *tok = strtok(buf, " \t");
  while (tok != NULL && argc < 8) {
    argv[argc++] = tok;
    tok = strtok(NULL, " \t");
  }

  if (argc == 0) return;

  // Some terminal clients can send control/escape sequences on connect.
  // Ignore non-command prefixes instead of emitting an error immediately.
  if (!isalpha((unsigned char)argv[0][0])) {
    command_write_line("ERR unknown-command");
    return;
  }

  // Flat dispatch keeps command latency deterministic and avoids heap use.
  if (strcmp(argv[0], "help") == 0) cmd_help();
  else if (strcmp(argv[0], "id") == 0) cmd_id();
  else if (strcmp(argv[0], "version") == 0) cmd_version();
  else if (strcmp(argv[0], "uptime") == 0) cmd_uptime();
  else if (strcmp(argv[0], "state") == 0) cmd_state();
  else if (strcmp(argv[0], "set") == 0) cmd_set(argv, argc);
  else if (strcmp(argv[0], "toggle") == 0) cmd_toggle(argv, argc);
  else if (strcmp(argv[0], "pulse") == 0) cmd_pulse(argv, argc);
  else if (strcmp(argv[0], "pulse-on") == 0) cmd_pulse_on(argv, argc);
  else if (strcmp(argv[0], "pulse-off") == 0) cmd_pulse_off(argv, argc);
  else if (strcmp(argv[0], "setmask") == 0) cmd_setmask(argv, argc);
  else if (strcmp(argv[0], "all") == 0) cmd_all(argv, argc);
  else if (strcmp(argv[0], "reboot-dfu") == 0) cmd_reboot_dfu();
  else command_write_line("ERR unknown-command");
}
