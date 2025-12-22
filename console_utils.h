#ifndef CONSOLE_UTILS_H
#define CONSOLE_UTILS_H

#include <stdarg.h>
#include "FreeRTOS.h"
#include "semphr.h"

#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"

extern SemaphoreHandle_t console_mutex;
extern SemaphoreHandle_t rng_mutex;

int thread_safe_rand(void);
void safe_printf(const char* color, const char* format, ...);

#endif // CONSOLE_UTILS_H
