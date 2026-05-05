#include <string.h>

#include "bsp/board_api.h"
#include "command.h"
#include "relay.h"
#include "tusb.h"

static char rx_line[128];
static uint32_t rx_len;

void tud_mount_cb(void) {
  // Host-visible readiness marker right after CDC mount completes.
  command_write_line("OK");
}

void tud_cdc_rx_cb(uint8_t itf) {
  (void)itf;
  // Collect a line-oriented command stream over CDC.
  while (tud_cdc_available()) {
    char c;
    tud_cdc_read(&c, 1);
    if (c == '\r' || c == '\n') {
      if (rx_len > 0) {
        // Dispatch only non-empty lines so CR/LF noise is ignored.
        rx_line[rx_len] = '\0';
        command_process_line(rx_line);
        rx_len = 0;
      }
    } else if (rx_len < sizeof(rx_line) - 1u) {
      rx_line[rx_len++] = c;
    } else {
      // Drop oversized lines to avoid partial command execution.
      rx_len = 0;
      command_write_line("error: command too long (max 127 chars)");
    }
  }
}

int main(void) {
  // Hardware first, then protocol state, then USB stack.
  board_init();
  relay_init();
  command_init();

  tusb_init();

  while (1) {
    // Cooperative main loop: USB IRQ work + command/relay scheduling.
    tud_task();
    command_poll();
    relay_pulse_poll(command_now_ms());
  }
  return 0;
}
