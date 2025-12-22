/*
 * Manufacturing Process Control System using FreeRTOS with Petri Net MoC
 * Configured for FreeRTOS Windows MSVC Port
 *
 * Manufacturing Line: Raw Material -> Processing -> Assembly -> Quality Check -> Packaging
 *
 * Petri Net Elements:
 * - Places: Represent states/buffers in the manufacturing process
 * - Transitions: Represent manufacturing operations/events
 * - Tokens: Represent workpieces/materials flowing through the system
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "petri_net.h"
#include "manufacturing_process.h"
#include "console_utils.h"
#include "tasks.h"
#include "status_server.h"

#if defined(_MSC_VER)
 // MSVC does not support C11 atomics in C mode
typedef volatile long atomic_bool;
#define atomic_store(ptr, val) (*(ptr) = (val))
#define atomic_load(ptr) (*(ptr))
#else
#include <stdatomic.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

/* FreeRTOS Windows MSVC port includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

// Declare extern globals
extern PetriNet manufacturing_net;
extern atomic_bool status_dirty;

// Define status_dirty here
atomic_bool status_dirty = false;

// Declare log_queue
QueueHandle_t log_queue;

/* Windows console color support */
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define STATUS_SERVER_PORT 8080
#define STATUS_JSON_BUFFER 2048
#define STATUS_RESPONSE_BUFFER (STATUS_JSON_BUFFER + 256)
#define STATUS_SERVER_BACKLOG 5

// ====================
// MAIN APPLICATION
// ====================

/**
 * @brief Main application entry point for the manufacturing process control system.
 */
void main_blinky(void) {
    // Enable Windows console colors (for Windows 10+)
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hConsole, &mode);
    SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    printf("\n");
    printf(COLOR_GREEN "===========================================================\n" COLOR_RESET);
    printf(COLOR_GREEN "|   MANUFACTURING PROCESS CONTROL SYSTEM                    |\n" COLOR_RESET);
    printf(COLOR_GREEN "|   Using FreeRTOS with Petri Net Model of Computation      |\n" COLOR_RESET);
    printf(COLOR_GREEN "|   Running on Windows MSVC Port                            |\n" COLOR_RESET);
    printf(COLOR_GREEN "===========================================================\n" COLOR_RESET);
    printf("\n");

    // Create console mutex
    console_mutex = xSemaphoreCreateMutex();
    if (console_mutex == NULL) {
        printf("ERROR: Failed to create console mutex\n");
        return;
    }

    rng_mutex = xSemaphoreCreateMutex();
    if (rng_mutex == NULL) {
        printf("ERROR: Failed to create RNG mutex\n");
        return;
    }

    // Initialize Petri net
    init_petri_net();
    setup_manufacturing_process();

    srand((unsigned int)time(NULL));

    printf(COLOR_YELLOW "System initialized with 20 raw materials\n" COLOR_RESET);
    printf(COLOR_YELLOW "Starting manufacturing tasks...\n\n" COLOR_RESET);

    // Create log queue
    log_queue = xQueueCreate(50, sizeof(char[64]));

    // Create FreeRTOS tasks for each manufacturing station
    // Using appropriate stack sizes for Windows port
    BaseType_t result;

    result = xTaskCreate(task_material_loader, "MaterialLoader",
        configMINIMAL_STACK_SIZE * 2, NULL, 3, NULL);
    if (result != pdPASS) {
        printf("ERROR: Failed to create MaterialLoader task\n");
        return;
    }

    result = xTaskCreate(task_processor, "Processor",
        configMINIMAL_STACK_SIZE * 2, NULL, 3, NULL);
    if (result != pdPASS) {
        printf("ERROR: Failed to create Processor task\n");
        return;
    }

    result = xTaskCreate(task_assembler, "Assembler",
        configMINIMAL_STACK_SIZE * 2, NULL, 3, NULL);
    if (result != pdPASS) {
        printf("ERROR: Failed to create Assembler task\n");
        return;
    }

    result = xTaskCreate(task_painter_router, "PainterRouter",
        configMINIMAL_STACK_SIZE * 2, NULL, 3, NULL);
    if (result != pdPASS) {
        printf("ERROR: Failed to create PainterRouter task\n");
        return;
    }

    result = xTaskCreate(task_quality_control, "QualityControl",
        configMINIMAL_STACK_SIZE * 2, NULL, 4, NULL);
    if (result != pdPASS) {
        printf("ERROR: Failed to create QualityControl task\n");
        return;
    }

    result = xTaskCreate(task_packager, "Packager",
        configMINIMAL_STACK_SIZE * 2, NULL, 3, NULL);
    if (result != pdPASS) {
        printf("ERROR: Failed to create Packager task\n");
        return;
    }

    result = xTaskCreate(task_reworker, "Reworker",
        configMINIMAL_STACK_SIZE * 2, NULL, 2, NULL);
    if (result != pdPASS) {
        printf("ERROR: Failed to create Reworker task\n");
        return;
    }

    result = xTaskCreate(task_status_server, "StatusServer",
        configMINIMAL_STACK_SIZE * 3, NULL, 2, NULL);
    if (result != pdPASS) {
        printf("ERROR: Failed to create StatusServer task\n");
        return;
    }

    // Start FreeRTOS scheduler
    printf(COLOR_GREEN "Starting FreeRTOS scheduler...\n\n" COLOR_RESET);
    vTaskStartScheduler();

    // Should never reach here unless there's insufficient heap
    printf(COLOR_RED "ERROR: Scheduler failed to start - insufficient heap memory?\n" COLOR_RESET);
}
// Stub for vBlinkyKeyboardInterruptHandler to resolve linker error
void vBlinkyKeyboardInterruptHandler(int xKeyPressed) {
    // Handle keyboard input to increase raw materials
    if (xKeyPressed == '+') {
        // Increase raw materials by 1
        xSemaphoreTake(manufacturing_net.places[P_RAW_MATERIAL].mutex, portMAX_DELAY);
        manufacturing_net.places[P_RAW_MATERIAL].tokens += 1;
        xSemaphoreGive(manufacturing_net.places[P_RAW_MATERIAL].mutex);

        // Mark status as dirty for update
        atomic_store(&status_dirty, true);

        // Print confirmation (thread-safe)
        safe_printf(COLOR_YELLOW, "[Keyboard] Increased raw materials by 1 (total: %d)\n",
            get_place_tokens(P_RAW_MATERIAL));
    }
}