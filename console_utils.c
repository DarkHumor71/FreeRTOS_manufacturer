#include "console_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

SemaphoreHandle_t console_mutex;
SemaphoreHandle_t rng_mutex;

int thread_safe_rand(void) {
    int result;
    xSemaphoreTake(rng_mutex, portMAX_DELAY);
    result = rand();
    xSemaphoreGive(rng_mutex);
    return result;
}

void safe_printf(const char* color, const char* format, ...) {
    va_list args;
    xSemaphoreTake(console_mutex, portMAX_DELAY);
    printf("%s", color);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("%s", COLOR_RESET);
    fflush(stdout);
    xSemaphoreGive(console_mutex);
}
