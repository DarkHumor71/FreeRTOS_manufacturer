#include "manufacturing_process.h"
#include "petri_net.h"

void setup_manufacturing_process(void) {
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
    add_place("Final Packaged", 0);                    // P10
    add_place("Painted", 0);                           // P11
    add_place("QC Active 2", 0);                       // P12
    add_place("Worker", 3);                            // P13
    add_place("Rework Bin", 0);                        // P14

    add_transition("Load Material");
    add_arc_input(T_LOAD_MATERIAL, P_RAW_MATERIAL, 1);
    add_arc_output(T_LOAD_MATERIAL, P_READY_TO_PROCESS, 1);

    add_transition("Start Processing");
    add_arc_input(T_START_PROCESSING, P_READY_TO_PROCESS, 1);
    add_arc_output(T_START_PROCESSING, P_PROCESSING, 1);

    add_transition("Finish Processing");
    add_arc_input(T_FINISH_PROCESSING, P_PROCESSING, 1);
    add_arc_output(T_FINISH_PROCESSING, P_PROCESSED, 1);

    add_transition("Start Assembly");
    add_arc_input(T_START_ASSEMBLY, P_PROCESSED, 2);
    add_arc_output(T_START_ASSEMBLY, P_READY_TO_ASSEMBLE, 2);

    add_transition("Finish Assembly");
    add_arc_input(T_FINISH_ASSEMBLY, P_READY_TO_ASSEMBLE, 2);
    add_arc_output(T_FINISH_ASSEMBLY, P_ASSEMBLED, 1);

    add_transition("Start QC 1");
    add_arc_input(T_START_QC_1, P_ASSEMBLED, 1);
    add_arc_input(T_START_QC_1, P_WORKER, 1);
    add_arc_output(T_START_QC_1, P_QUALITY_CHECK_1, 1);

    add_transition("Pass QC 1");
    add_arc_input(T_PASS_QC_1, P_QUALITY_CHECK_1, 1);
    add_arc_output(T_PASS_QC_1, P_POST_QC1_BUFFER, 1);
    add_arc_output(T_PASS_QC_1, P_WORKER, 1);

    add_transition("Fail QC 1");
    add_arc_input(T_FAIL_QC_1, P_QUALITY_CHECK_1, 1);
    add_arc_output(T_FAIL_QC_1, P_REWORK_BIN, 1);
    add_arc_output(T_FAIL_QC_1, P_WORKER, 1);

    add_transition("Select to Paint");
    add_arc_input(T_SELECT_TO_PAINT, P_POST_QC1_BUFFER, 1);
    add_arc_output(T_SELECT_TO_PAINT, P_PAINTED, 1);

    add_transition("Skip Paint");
    add_arc_input(T_SKIP_PAINT, P_POST_QC1_BUFFER, 1);
    add_arc_output(T_SKIP_PAINT, P_READY_FOR_INDIVIDUAL_PACKAGE, 1);

    add_transition("Start QC 2");
    add_arc_input(T_START_QC_2, P_PAINTED, 1);
    add_arc_input(T_START_QC_2, P_WORKER, 1);
    add_arc_output(T_START_QC_2, P_QUALITY_CHECK_2, 1);

    add_transition("Pass QC 2");
    add_arc_input(T_PASS_QC_2, P_QUALITY_CHECK_2, 1);
    add_arc_output(T_PASS_QC_2, P_READY_FOR_INDIVIDUAL_PACKAGE, 1);
    add_arc_output(T_PASS_QC_2, P_WORKER, 1);

    add_transition("Fail QC 2");
    add_arc_input(T_FAIL_QC_2, P_QUALITY_CHECK_2, 1);
    add_arc_output(T_FAIL_QC_2, P_REWORK_BIN, 1);
    add_arc_output(T_FAIL_QC_2, P_WORKER, 1);

    add_transition("Individual Package");
    add_arc_input(T_INDIVIDUAL_PACKAGE, P_READY_FOR_INDIVIDUAL_PACKAGE, 1);
    add_arc_output(T_INDIVIDUAL_PACKAGE, P_INDIVIDUALLY_PACKAGED, 1);

    add_transition("Bulk Package");
    add_arc_input(T_BULK_PACKAGE, P_INDIVIDUALLY_PACKAGED, 5);
    add_arc_output(T_BULK_PACKAGE, P_FINAL_PACKAGED, 1);

    add_transition("Rework Process");
    add_arc_input(T_REWORK_PROCESS, P_REWORK_BIN, 1);
    add_arc_input(T_REWORK_PROCESS, P_WORKER, 1);
    add_arc_output(T_REWORK_PROCESS, P_PROCESSED, 1);
    add_arc_output(T_REWORK_PROCESS, P_WORKER, 1);
}
