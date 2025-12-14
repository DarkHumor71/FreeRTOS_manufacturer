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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>

 /* FreeRTOS Windows MSVC port includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/* Windows console color support */
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"

// ====================
// PETRI NET STRUCTURE
// ====================

#define MAX_PLACES 10
#define MAX_TRANSITIONS 8
#define MAX_ARCS 20

typedef struct {
    int tokens;                    // Number of tokens in the place
    char name[32];                 // Place name
    SemaphoreHandle_t mutex;       // Mutex for thread-safe access
} Place;

typedef struct {
    int input_places[5];           // Input place indices (-1 = not used)
    int output_places[5];          // Output place indices (-1 = not used)
    int input_weights[5];          // Tokens required from each input
    int output_weights[5];         // Tokens produced to each output
    char name[32];                 // Transition name
    bool enabled;                  // Transition enabled status
} Transition;

typedef struct {
    Place places[MAX_PLACES];
    Transition transitions[MAX_TRANSITIONS];
    int num_places;
    int num_transitions;
    SemaphoreHandle_t net_mutex;   // Global Petri net mutex
} PetriNet;

// Global Petri Net
PetriNet manufacturing_net;

// Queue for logging events
QueueHandle_t log_queue;

// Mutex for console output (prevents interleaved prints)
SemaphoreHandle_t console_mutex;

// ====================
// CONSOLE OUTPUT HELPERS
// ====================

/**
 * @brief Thread-safe printf with color support for Windows console.
 * @param color ANSI color code string.
 * @param format printf-style format string.
 * @param ... Variable arguments for formatting.
 */
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

// ====================
// PETRI NET OPERATIONS
// ====================

/**
 * @brief Initialize the Petri net structure, places, transitions, and mutexes.
 */
void init_petri_net(void) {
    manufacturing_net.num_places = 0;
    manufacturing_net.num_transitions = 0;
    manufacturing_net.net_mutex = xSemaphoreCreateMutex();

    if (manufacturing_net.net_mutex == NULL) {
        printf("ERROR: Failed to create net mutex\n");
        exit(1);
    }

    for (int i = 0; i < MAX_PLACES; i++) {
        manufacturing_net.places[i].tokens = 0;
        manufacturing_net.places[i].mutex = xSemaphoreCreateMutex();

        if (manufacturing_net.places[i].mutex == NULL) {
            printf("ERROR: Failed to create place mutex %d\n", i);
            exit(1);
        }
    }

    for (int i = 0; i < MAX_TRANSITIONS; i++) {
        for (int j = 0; j < 5; j++) {
            manufacturing_net.transitions[i].input_places[j] = -1;
            manufacturing_net.transitions[i].output_places[j] = -1;
            manufacturing_net.transitions[i].input_weights[j] = 0;
            manufacturing_net.transitions[i].output_weights[j] = 0;
        }
        manufacturing_net.transitions[i].enabled = false;
    }
}

/**
 * @brief Add a new place to the Petri net.
 * @param name Name of the place.
 * @param initial_tokens Initial number of tokens in the place.
 * @return Index of the new place.
 */
int add_place(const char* name, int initial_tokens) {
    int idx = manufacturing_net.num_places++;
    snprintf(manufacturing_net.places[idx].name, 32, "%s", name);
    manufacturing_net.places[idx].tokens = initial_tokens;
    return idx;
}

/**
 * @brief Add a new transition to the Petri net.
 * @param name Name of the transition.
 * @return Index of the new transition.
 */
int add_transition(const char* name) {
    int idx = manufacturing_net.num_transitions++;
    snprintf(manufacturing_net.transitions[idx].name, 32, "%s", name);
    return idx;
}

/**
 * @brief Add an input arc from a place to a transition.
 * @param trans_idx Index of the transition.
 * @param place_idx Index of the input place.
 * @param weight Number of tokens required from the place.
 */
void add_arc_input(int trans_idx, int place_idx, int weight) {
    for (int i = 0; i < 5; i++) {
        if (manufacturing_net.transitions[trans_idx].input_places[i] == -1) {
            manufacturing_net.transitions[trans_idx].input_places[i] = place_idx;
            manufacturing_net.transitions[trans_idx].input_weights[i] = weight;
            break;
        }
    }
}

/**
 * @brief Add an output arc from a transition to a place.
 * @param trans_idx Index of the transition.
 * @param place_idx Index of the output place.
 * @param weight Number of tokens produced to the place.
 */
void add_arc_output(int trans_idx, int place_idx, int weight) {
    for (int i = 0; i < 5; i++) {
        if (manufacturing_net.transitions[trans_idx].output_places[i] == -1) {
            manufacturing_net.transitions[trans_idx].output_places[i] = place_idx;
            manufacturing_net.transitions[trans_idx].output_weights[i] = weight;
            break;
        }
    }
}

/**
 * @brief Check if a transition is enabled (all input places have required tokens).
 * @param trans_idx Index of the transition.
 * @return true if enabled, false otherwise.
 */
bool is_transition_enabled(int trans_idx) {
    Transition* t = &manufacturing_net.transitions[trans_idx];

    for (int i = 0; i < 5; i++) {
        if (t->input_places[i] == -1) break;

        int place_idx = t->input_places[i];
        int required = t->input_weights[i];

        xSemaphoreTake(manufacturing_net.places[place_idx].mutex, portMAX_DELAY);
        int available = manufacturing_net.places[place_idx].tokens;
        xSemaphoreGive(manufacturing_net.places[place_idx].mutex);

        if (available < required) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Fire a transition: consume tokens from input places and produce tokens to output places.
 * @param trans_idx Index of the transition.
 * @return true if fired successfully, false otherwise.
 */
bool fire_transition(int trans_idx) {
    Transition* t = &manufacturing_net.transitions[trans_idx];

    xSemaphoreTake(manufacturing_net.net_mutex, portMAX_DELAY);

    // Check if enabled
    if (!is_transition_enabled(trans_idx)) {
        xSemaphoreGive(manufacturing_net.net_mutex);
        return false;
    }

    // Remove tokens from input places
    for (int i = 0; i < 5; i++) {
        if (t->input_places[i] == -1) break;

        int place_idx = t->input_places[i];
        int weight = t->input_weights[i];

        xSemaphoreTake(manufacturing_net.places[place_idx].mutex, portMAX_DELAY);
        manufacturing_net.places[place_idx].tokens -= weight;
        xSemaphoreGive(manufacturing_net.places[place_idx].mutex);
    }

    // Add tokens to output places
    for (int i = 0; i < 5; i++) {
        if (t->output_places[i] == -1) break;

        int place_idx = t->output_places[i];
        int weight = t->output_weights[i];

        xSemaphoreTake(manufacturing_net.places[place_idx].mutex, portMAX_DELAY);
        manufacturing_net.places[place_idx].tokens += weight;
        xSemaphoreGive(manufacturing_net.places[place_idx].mutex);
    }

    xSemaphoreGive(manufacturing_net.net_mutex);

    return true;
}

/**
 * @brief Get the number of tokens in a place.
 * @param place_idx Index of the place.
 * @return Number of tokens in the place.
 */
int get_place_tokens(int place_idx) {
    xSemaphoreTake(manufacturing_net.places[place_idx].mutex, portMAX_DELAY);
    int tokens = manufacturing_net.places[place_idx].tokens;
    xSemaphoreGive(manufacturing_net.places[place_idx].mutex);
    return tokens;
}

// ====================
// MANUFACTURING PROCESS DEFINITION
// ====================

// Place indices
enum Places {
    P_RAW_MATERIAL,
    P_READY_TO_PROCESS,
    P_PROCESSING,
    P_PROCESSED,
    P_READY_TO_ASSEMBLE,
    P_ASSEMBLY,
    P_ASSEMBLED,
    P_QUALITY_CHECK,
    P_PASSED_QC,
    P_PACKAGING
};

// Transition indices
enum Transitions {
    T_LOAD_MATERIAL,
    T_START_PROCESSING,
    T_FINISH_PROCESSING,
    T_START_ASSEMBLY,
    T_FINISH_ASSEMBLY,
    T_START_QC,
    T_PASS_QC,
    T_PACKAGE
};

/**
 * @brief Set up the manufacturing process Petri net: places, transitions, and arcs.
 */
void setup_manufacturing_process(void) {
    // Create places
    add_place("Raw Material", 10);          // P0: Initial raw materials
    add_place("Ready to Process", 0);       // P1
    add_place("Processing", 0);             // P2
    add_place("Processed", 0);              // P3
    add_place("Ready to Assemble", 0);      // P4
    add_place("Assembly", 0);               // P5
    add_place("Assembled", 0);              // P6
    add_place("Quality Check", 0);          // P7
    add_place("Passed QC", 0);              // P8
    add_place("Packaging", 0);              // P9

    // Create transitions and arcs

    // T0: Load Material (P0 -> P1)
    add_transition("Load Material");
    add_arc_input(T_LOAD_MATERIAL, P_RAW_MATERIAL, 1);
    add_arc_output(T_LOAD_MATERIAL, P_READY_TO_PROCESS, 1);

    // T1: Start Processing (P1 -> P2)
    add_transition("Start Processing");
    add_arc_input(T_START_PROCESSING, P_READY_TO_PROCESS, 1);
    add_arc_output(T_START_PROCESSING, P_PROCESSING, 1);

    // T2: Finish Processing (P2 -> P3)
    add_transition("Finish Processing");
    add_arc_input(T_FINISH_PROCESSING, P_PROCESSING, 1);
    add_arc_output(T_FINISH_PROCESSING, P_PROCESSED, 1);

    // T3: Start Assembly (P3 -> P4, requires 2 processed items)
    add_transition("Start Assembly");
    add_arc_input(T_START_ASSEMBLY, P_PROCESSED, 2);
    add_arc_output(T_START_ASSEMBLY, P_READY_TO_ASSEMBLE, 1);

    // T4: Finish Assembly (P4 -> P5)
    add_transition("Finish Assembly");
    add_arc_input(T_FINISH_ASSEMBLY, P_READY_TO_ASSEMBLE, 1);
    add_arc_output(T_FINISH_ASSEMBLY, P_ASSEMBLED, 1);

    // T5: Start Quality Check (P5 -> P6)
    add_transition("Start QC");
    add_arc_input(T_START_QC, P_ASSEMBLED, 1);
    add_arc_output(T_START_QC, P_QUALITY_CHECK, 1);

    // T6: Pass Quality Check (P6 -> P7)
    add_transition("Pass QC");
    add_arc_input(T_PASS_QC, P_QUALITY_CHECK, 1);
    add_arc_output(T_PASS_QC, P_PASSED_QC, 1);

    // T7: Package (P7 -> P8)
    add_transition("Package Product");
    add_arc_input(T_PACKAGE, P_PASSED_QC, 1);
    add_arc_output(T_PACKAGE, P_PACKAGING, 1);
}

// ====================
// FREERTOS TASKS
// ====================

/**
 * @brief FreeRTOS task: Loads raw material into the process.
 * @param params Unused task parameter.
 */
void task_material_loader(void* params) {
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        if (fire_transition(T_LOAD_MATERIAL)) {
            safe_printf(COLOR_CYAN, "[Material Loader] Loaded raw material -> Ready to Process\n");
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(800));
    }
}

/**
 * @brief FreeRTOS task: Processes raw material.
 * @param params Unused task parameter.
 */
void task_processor(void* params) {
    TickType_t last_wake = xTaskGetTickCount();
    int processed_count = 0;

    while (1) {
        if (fire_transition(T_START_PROCESSING)) {
            processed_count++;
            safe_printf(COLOR_BLUE, "[Processor] Started processing item #%d\n", processed_count);

            // Simulate processing time
            vTaskDelay(pdMS_TO_TICKS(1500));

            if (fire_transition(T_FINISH_PROCESSING)) {
                safe_printf(COLOR_BLUE, "[Processor] Finished processing item #%d\n", processed_count);
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(300));
    }
}

/**
 * @brief FreeRTOS task: Assembles processed items.
 * @param params Unused task parameter.
 */
void task_assembler(void* params) {
    TickType_t last_wake = xTaskGetTickCount();
    int assembled_count = 0;

    while (1) {
        if (fire_transition(T_START_ASSEMBLY)) {
            assembled_count++;
            safe_printf(COLOR_MAGENTA, "[Assembler] Started assembly #%d (combining 2 processed items)\n",
                assembled_count);

            // Simulate assembly time
            vTaskDelay(pdMS_TO_TICKS(1200));

            if (fire_transition(T_FINISH_ASSEMBLY)) {
                safe_printf(COLOR_MAGENTA, "[Assembler] Finished assembly #%d\n", assembled_count);
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(300));
    }
}

/**
 * @brief FreeRTOS task: Performs quality control on assembled items.
 * @param params Unused task parameter.
 */
void task_quality_control(void* params) {
    TickType_t last_wake = xTaskGetTickCount();
    int qc_count = 0;

    while (1) {
        if (fire_transition(T_START_QC)) {
            qc_count++;
            safe_printf(COLOR_YELLOW, "[QC] Started quality check #%d\n", qc_count);

            // Simulate QC time
            vTaskDelay(pdMS_TO_TICKS(800));

            if (fire_transition(T_PASS_QC)) {
                safe_printf(COLOR_GREEN, "[QC] Product #%d PASSED quality check\n", qc_count);
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(300));
    }
}

/**
 * @brief FreeRTOS task: Packages products that passed quality control.
 * @param params Unused task parameter.
 */
void task_packager(void* params) {
    TickType_t last_wake = xTaskGetTickCount();
    int packaged_count = 0;

    while (1) {
        if (fire_transition(T_PACKAGE)) {
            packaged_count++;
            safe_printf(COLOR_GREEN, "[Packager] ✓ Packaged product #%d - READY FOR SHIPMENT\n",
                packaged_count);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(600));
    }
}

/**
 * @brief FreeRTOS task: Monitors and prints the status of the manufacturing system.
 * @param params Unused task parameter.
 */
void task_monitor(void* params) {
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5000));

        xSemaphoreTake(console_mutex, portMAX_DELAY);

        printf("\n");
        printf(COLOR_CYAN "╔════════════════════════════════════════════════════════╗\n" COLOR_RESET);
        printf(COLOR_CYAN "║           MANUFACTURING SYSTEM STATUS                  ║\n" COLOR_RESET);
        printf(COLOR_CYAN "╠════════════════════════════════════════════════════════╣\n" COLOR_RESET);

        for (int i = 0; i < manufacturing_net.num_places; i++) {
            int tokens = get_place_tokens(i);
            printf(COLOR_CYAN "║ " COLOR_RESET);
            printf("%-30s: %2d tokens", manufacturing_net.places[i].name, tokens);
            printf(COLOR_CYAN "        ║\n" COLOR_RESET);
        }

        printf(COLOR_CYAN "╚════════════════════════════════════════════════════════╝\n" COLOR_RESET);
        printf("\n");
        fflush(stdout);

        xSemaphoreGive(console_mutex);
    }
}

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
    printf(COLOR_GREEN "╔═══════════════════════════════════════════════════════════╗\n" COLOR_RESET);
    printf(COLOR_GREEN "║   MANUFACTURING PROCESS CONTROL SYSTEM                    ║\n" COLOR_RESET);
    printf(COLOR_GREEN "║   Using FreeRTOS with Petri Net Model of Computation      ║\n" COLOR_RESET);
    printf(COLOR_GREEN "║   Running on Windows MSVC Port                            ║\n" COLOR_RESET);
    printf(COLOR_GREEN "╚═══════════════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");

    // Create console mutex
    console_mutex = xSemaphoreCreateMutex();
    if (console_mutex == NULL) {
        printf("ERROR: Failed to create console mutex\n");
        return;
    }

    // Initialize Petri net
    init_petri_net();
    setup_manufacturing_process();

    printf(COLOR_YELLOW "System initialized with 10 raw materials\n" COLOR_RESET);
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

    result = xTaskCreate(task_quality_control, "QualityControl",
        configMINIMAL_STACK_SIZE * 2, NULL, 3, NULL);
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

    result = xTaskCreate(task_monitor, "Monitor",
        configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL);
    if (result != pdPASS) {
        printf("ERROR: Failed to create Monitor task\n");
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
    // Not used in this demo
}