#include "tasks.h"
#include "petri_net.h"
#include "manufacturing_process.h"
#include "console_utils.h"
#include "FreeRTOS.h"
#include "task.h"

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
 * FIXED VERSION: Eliminates TOCTOU race condition
 */
void task_painter_router(void* params) {
    const int paint_chance_percent = 30; // 30% chance to be selected for paint
    TickType_t last_wake = xTaskGetTickCount();
    int paint_count = 0;

    while (1) {
        // Check if paint selection is possible (eliminates race condition)
        if (is_transition_enabled(T_SELECT_TO_PAINT)) {
            // Random Decision: Paint or Skip
            if ((thread_safe_rand() % 100) < paint_chance_percent) {
                // Decision: Paint
                if (fire_transition(T_SELECT_TO_PAINT)) {
                    paint_count++;
                    safe_printf(COLOR_MAGENTA, "[Router] Item #%d selected for custom paint.\n", paint_count);
                    vTaskDelay(pdMS_TO_TICKS(1500)); // Simulate Painting Time
                    safe_printf(COLOR_MAGENTA, "[Router] Item #%d finished painting -> Waiting for QC2.\n", paint_count);
                } else {
                    safe_printf(COLOR_RED, "[Router] ERROR: Failed to select item for painting\n");
                }
            } else {
                // Decision: Skip Paint - check if skip is enabled
                if (is_transition_enabled(T_SKIP_PAINT)) {
                    if (fire_transition(T_SKIP_PAINT)) {
                        safe_printf(COLOR_CYAN, "[Router] Item skipped paint -> Direct to Packaging.\n");
                    } else {
                        safe_printf(COLOR_RED, "[Router] ERROR: Failed to skip painting\n");
                    }
                }
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(400));
    }
}
/**
 * @brief FreeRTOS task: Performs quality control on assembled items.
 * @param params Unused task parameter.
  * @brief FreeRTOS task: Performs quality control (both QC1 and QC2).
  */
void task_quality_control(void* params) {
    const TickType_t qc_duration = pdMS_TO_TICKS(3000);
    const int fail_chance_percent = 5;
    TickType_t last_wake = xTaskGetTickCount();
    int qc_count = 0;

    while (1) {
        bool worked = false;
        int start_transition = -1;
        int pass_transition = -1;
        int fail_transition = -1;

        // PRIORITY 1: Post-paint QC2 takes precedence
        if (is_transition_enabled(T_START_QC_2)) {
            start_transition = T_START_QC_2;
            pass_transition = T_PASS_QC_2;
            fail_transition = T_FAIL_QC_2;
            worked = true;
        }
        // PRIORITY 2: Pre-paint QC1
        else if (is_transition_enabled(T_START_QC_1)) {
            start_transition = T_START_QC_1;
            pass_transition = T_PASS_QC_1;
            fail_transition = T_FAIL_QC_1;
            worked = true;
        }

        if (worked) {
            // Fire the start transition
            if (!fire_transition(start_transition)) {
                safe_printf(COLOR_RED, "[QC Worker] ERROR: Failed to start QC check\n");
                continue;
            }

            qc_count++;
            safe_printf(COLOR_YELLOW, "[QC Worker] Performing check #%d...\n", qc_count);
            vTaskDelay(qc_duration);

            // Determine pass/fail and fire appropriate transition
            int result_transition = ((thread_safe_rand() % 100) < fail_chance_percent) ? fail_transition : pass_transition;

            if (!fire_transition(result_transition)) {
                safe_printf(COLOR_RED, "[QC Worker] ERROR: Failed to complete QC check #%d\n", qc_count);
                continue;
            }

            if (result_transition == fail_transition) {
                safe_printf(COLOR_RED, "[QC Worker] Check #%d FAILED (5%% chance) -> Rework Bin\n", qc_count);
            } else {
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