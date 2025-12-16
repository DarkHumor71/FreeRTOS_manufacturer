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
#include <time.h>
#include <stdarg.h>

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

#define MAX_PLACES 15
#define MAX_TRANSITIONS 20
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
SemaphoreHandle_t rng_mutex;

// ====================
// CONSOLE OUTPUT HELPERS
// ====================

static int thread_safe_rand(void) {
    int result;
    xSemaphoreTake(rng_mutex, portMAX_DELAY);
    result = rand();
    xSemaphoreGive(rng_mutex);
    return result;
}

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
    if (manufacturing_net.num_places >= MAX_PLACES) {
        printf("ERROR: Cannot add place '%s' - max places reached\n", name);
        return -1;
    }

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
    if (manufacturing_net.num_transitions >= MAX_TRANSITIONS) {
        printf("ERROR: Cannot add transition '%s' - max transitions reached\n", name);
        return -1;
    }

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
    P_RAW_MATERIAL,            // P0
    P_READY_TO_PROCESS,        // P1
    P_PROCESSING,              // P2
    P_PROCESSED,               // P3
    P_READY_TO_ASSEMBLE,       // P4
    P_ASSEMBLED,               // P5
    P_QUALITY_CHECK_1,         // P6
    P_POST_QC1_BUFFER,         // P7
    P_READY_FOR_INDIVIDUAL_PACKAGE, // P8
    P_INDIVIDUALLY_PACKAGED,   // P9
    P_FINAL_PACKAGED,          // P10
    P_PAINTED,                 // P11
    P_QUALITY_CHECK_2,         // P12
    P_WORKER,                  // P13 - QC worker resource token
    P_REWORK_BIN               // P14
};

// Transition indices
enum Transitions {
    T_LOAD_MATERIAL,
    T_START_PROCESSING,
    T_FINISH_PROCESSING,
    T_START_ASSEMBLY,
    T_FINISH_ASSEMBLY,
    T_START_QC_1,
    T_PASS_QC_1,
    T_FAIL_QC_1,
    T_SELECT_TO_PAINT,
    T_SKIP_PAINT,
    T_START_QC_2,
    T_PASS_QC_2,
    T_FAIL_QC_2,
    T_INDIVIDUAL_PACKAGE,
    T_BULK_PACKAGE,
    T_REWORK_PROCESS
};

/**
 * @brief Set up the manufacturing process Petri net: places, transitions, and arcs.
 */
void setup_manufacturing_process(void) {
    // Create places
    add_place("Raw Material", 20);                     // P0
    add_place("Ready to Process", 0);                  // P1
    add_place("Processing", 0);                        // P2
    add_place("Processed", 0);                         // P3
    add_place("Ready to Assemble", 0);                 // P4
    add_place("Assembled", 0);                         // P5
    add_place("QC Active 1", 0);                       // P6
    add_place("Passed QC1 / Decision", 0);             // P7
    add_place("Ready for Individual Package", 0);      // P8
    add_place("Individually Packaged", 0);             // P9
    add_place("Final Packaged", 0);                     // P10
    add_place("Painted", 0);                           // P11
    add_place("QC Active 2", 0);                       // P12
    add_place("Worker", 1);                            // P13
    add_place("Rework Bin", 0);                        // P14

    // Transitions (order must match enum indices)

    // T0: Load Material
    add_transition("Load Material");
    add_arc_input(T_LOAD_MATERIAL, P_RAW_MATERIAL, 1);
    add_arc_output(T_LOAD_MATERIAL, P_READY_TO_PROCESS, 1);

    // T1: Start Processing
    add_transition("Start Processing");
    add_arc_input(T_START_PROCESSING, P_READY_TO_PROCESS, 1);
    add_arc_output(T_START_PROCESSING, P_PROCESSING, 1);

    // T2: Finish Processing
    add_transition("Finish Processing");
    add_arc_input(T_FINISH_PROCESSING, P_PROCESSING, 1);
    add_arc_output(T_FINISH_PROCESSING, P_PROCESSED, 1);

    // T3: Start Assembly
    add_transition("Start Assembly");
    add_arc_input(T_START_ASSEMBLY, P_PROCESSED, 2);
    add_arc_output(T_START_ASSEMBLY, P_READY_TO_ASSEMBLE, 1);

    // T4: Finish Assembly
    add_transition("Finish Assembly");
    add_arc_input(T_FINISH_ASSEMBLY, P_READY_TO_ASSEMBLE, 1);
    add_arc_output(T_FINISH_ASSEMBLY, P_ASSEMBLED, 1);

    // T5: Start QC1
    add_transition("Start QC 1");
    add_arc_input(T_START_QC_1, P_ASSEMBLED, 1);
    add_arc_input(T_START_QC_1, P_WORKER, 1);
    add_arc_output(T_START_QC_1, P_QUALITY_CHECK_1, 1);

    // T6: Pass QC1
    add_transition("Pass QC 1");
    add_arc_input(T_PASS_QC_1, P_QUALITY_CHECK_1, 1);
    add_arc_output(T_PASS_QC_1, P_POST_QC1_BUFFER, 1);
    add_arc_output(T_PASS_QC_1, P_WORKER, 1);

    // T7: Fail QC1
    add_transition("Fail QC 1");
    add_arc_input(T_FAIL_QC_1, P_QUALITY_CHECK_1, 1);
    add_arc_output(T_FAIL_QC_1, P_REWORK_BIN, 1);
    add_arc_output(T_FAIL_QC_1, P_WORKER, 1);

    // T8: Select to Paint
    add_transition("Select to Paint");
    add_arc_input(T_SELECT_TO_PAINT, P_POST_QC1_BUFFER, 1);
    add_arc_output(T_SELECT_TO_PAINT, P_PAINTED, 1);

    // T9: Skip Paint
    add_transition("Skip Paint");
    add_arc_input(T_SKIP_PAINT, P_POST_QC1_BUFFER, 1);
    add_arc_output(T_SKIP_PAINT, P_READY_FOR_INDIVIDUAL_PACKAGE, 1);

    // T10: Start QC2
    add_transition("Start QC 2");
    add_arc_input(T_START_QC_2, P_PAINTED, 1);
    add_arc_input(T_START_QC_2, P_WORKER, 1);
    add_arc_output(T_START_QC_2, P_QUALITY_CHECK_2, 1);

    // T11: Pass QC2
    add_transition("Pass QC 2");
    add_arc_input(T_PASS_QC_2, P_QUALITY_CHECK_2, 1);
    add_arc_output(T_PASS_QC_2, P_READY_FOR_INDIVIDUAL_PACKAGE, 1);
    add_arc_output(T_PASS_QC_2, P_WORKER, 1);

    // T12: Fail QC2
    add_transition("Fail QC 2");
    add_arc_input(T_FAIL_QC_2, P_QUALITY_CHECK_2, 1);
    add_arc_output(T_FAIL_QC_2, P_REWORK_BIN, 1);
    add_arc_output(T_FAIL_QC_2, P_WORKER, 1);

    // T13: Individual Package
    add_transition("Individual Package");
    add_arc_input(T_INDIVIDUAL_PACKAGE, P_READY_FOR_INDIVIDUAL_PACKAGE, 1);
    add_arc_output(T_INDIVIDUAL_PACKAGE, P_INDIVIDUALLY_PACKAGED, 1);

    // T14: Bulk Package (requires 5 individual packages)
    add_transition("Bulk Package");
    add_arc_input(T_BULK_PACKAGE, P_INDIVIDUALLY_PACKAGED, 5);
    add_arc_output(T_BULK_PACKAGE, P_FINAL_PACKAGED, 1);

    // T15: Rework Process
    add_transition("Rework Process");
    add_arc_input(T_REWORK_PROCESS, P_REWORK_BIN, 1);
    add_arc_output(T_REWORK_PROCESS, P_PROCESSED, 1);
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
 * @brief FreeRTOS task: Routes product after QC1, randomly selecting some for painting.
 */
void task_painter_router(void* params) {
    const int paint_chance_percent = 30; // 30% chance to be selected for paint
    TickType_t last_wake = xTaskGetTickCount();
    int paint_count = 0;

    while (1) {
        // Check if item is waiting at the decision point (P_POST_QC1_BUFFER)
        if (get_place_tokens(P_POST_QC1_BUFFER) > 0) {

            // Random Decision: Paint or Skip
            if ((thread_safe_rand() % 100) < paint_chance_percent) {
                // Decision: Paint
                if (fire_transition(T_SELECT_TO_PAINT)) {
                    paint_count++;
                    safe_printf(COLOR_MAGENTA, "[Router] Item #%d selected for custom paint.\n", paint_count);
                    vTaskDelay(pdMS_TO_TICKS(1500)); // Simulate Painting Time
                    safe_printf(COLOR_MAGENTA, "[Router] Item #%d finished painting -> Waiting for QC2.\n", paint_count);
                }
            }
            else {
                // Decision: Skip Paint
                if (fire_transition(T_SKIP_PAINT)) {
                    safe_printf(COLOR_CYAN, "[Router] Item skipped paint -> Direct to Packaging.\n");
                }
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(400));
    }
}
/**
 * @brief FreeRTOS task: Performs quality control on assembled items.
 * @param params Unused task parameter.
 */
 /**
  * @brief FreeRTOS task: Performs quality control (both QC1 and QC2).
  */
void task_quality_control(void* params) {
    const TickType_t qc_duration = pdMS_TO_TICKS(1000);
    const int fail_chance_percent = 5;
    TickType_t last_wake = xTaskGetTickCount();
    int qc_count = 0;

    while (1) {
        bool worked = false;
        int pass_transition = -1;
        int fail_transition = -1;

        // PRIORITY 1: Post-paint QC2 takes precedence
        if (fire_transition(T_START_QC_2)) {
            safe_printf(COLOR_YELLOW, "[QC Worker] Performing POST-PAINT check #%d...\n", qc_count + 1);
            vTaskDelay(qc_duration);
            pass_transition = T_PASS_QC_2;
            fail_transition = T_FAIL_QC_2;
            worked = true;
        }
        // PRIORITY 2: Pre-paint QC1
        else if (fire_transition(T_START_QC_1)) {
            safe_printf(COLOR_YELLOW, "[QC Worker] Performing PRE-PAINT check #%d...\n", qc_count + 1);
            vTaskDelay(qc_duration);
            pass_transition = T_PASS_QC_1;
            fail_transition = T_FAIL_QC_1;
            worked = true;
        }

        if (worked) {
            qc_count++;
            if ((thread_safe_rand() % 100) < fail_chance_percent) {
                fire_transition(fail_transition);
                safe_printf(COLOR_RED, "[QC Worker] Check #%d FAILED (5%% chance) -> Rework Bin\n", qc_count);
            } else {
                fire_transition(pass_transition);
                safe_printf(COLOR_GREEN, "[QC Worker] Check #%d PASSED -> Next Stage\n", qc_count);
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(worked ? 200 : 500));
    }
}

void task_reworker(void* params) {
    TickType_t last_wake = xTaskGetTickCount();
    int rework_count = 0;
    const TickType_t rework_duration = pdMS_TO_TICKS(2500);

    while (1) {
        if (fire_transition(T_REWORK_PROCESS)) {
            rework_count++;
            safe_printf(COLOR_BLUE, "[Reworker] Started rework #%d -> Back to Processed\n", rework_count);
            vTaskDelay(rework_duration);
            safe_printf(COLOR_BLUE, "[Reworker] Finished rework #%d\n", rework_count);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief FreeRTOS task: Packages products that passed quality control.
 * @param params Unused task parameter.
 */
void task_packager(void* params) {
    TickType_t last_wake = xTaskGetTickCount();
    int individual_count = 0;
    int bulk_count = 0;

    while (1) {
        bool worked = false;

        if (fire_transition(T_BULK_PACKAGE)) {
            bulk_count++;
            safe_printf(COLOR_GREEN, "[Packager] BULK PACKAGED unit #%d (5 individual units combined) -> READY FOR SHIPMENT\n", bulk_count);
            worked = true;
        } else if (fire_transition(T_INDIVIDUAL_PACKAGE)) {
            individual_count++;
            safe_printf(COLOR_BLUE, "[Packager] Individually packaged unit #%d. Waiting for 5 to form a bulk package...\n", individual_count);
            worked = true;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(worked ? 300 : 600));
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
        printf(COLOR_CYAN "===========================================================\n" COLOR_RESET);
        printf(COLOR_CYAN "|            MANUFACTURING SYSTEM STATUS                  |\n" COLOR_RESET);
        printf(COLOR_CYAN "===========================================================\n" COLOR_RESET);

        for (int i = 0; i < manufacturing_net.num_places; i++) {
            int tokens = get_place_tokens(i);
            printf(COLOR_CYAN "| " COLOR_RESET);
            printf("%-30s: %2d tokens", manufacturing_net.places[i].name, tokens);
            printf(COLOR_CYAN "        |\n" COLOR_RESET);
        }

        printf(COLOR_CYAN "===========================================================\n" COLOR_RESET);
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