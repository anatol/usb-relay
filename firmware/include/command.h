#pragma once

#include <stdint.h>

void command_init(void);
void command_process_line(const char *line);
void command_poll(void);

void command_write_line(const char *line);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
void command_writef(const char *fmt, ...);

uint32_t command_now_ms(void);
