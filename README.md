# FreeRTOS Manufacturing Process Control Demo

This demo implements a manufacturing process control system using a Petri net model of computation atop the FreeRTOS Windows MSVC port. The system routes raw materials through processing, assembly, quality control, optional paint, and packaging, while capturing resource constraints such as worker availability and rework flow.

## Architecture

- **Petri net topology** defines 15 places and 16 transitions representing production buffers, workers, inspections, painting decisions, and rework back into the line.
- **Shared synchronization** uses FreeRTOS mutexes to guard place token counts and a global net mutex to serialize transition firing.
- **Console output** is colored and serialized via a dedicated `console_mutex` to keep log lines readable on Windows terminals.
- **Logging queue** allocates a placeholder `log_queue` that can be extended for structured diagnostics in future enhancements.

## Key Tasks

| Task | Responsibility |
| --- | --- |
| `task_material_loader` | Feeds raw material tokens into the processing queue every 800 ms. |
| `task_processor` | Starts and completes processing cycles, producing processed items and simulating work durations. |
| `task_assembler` | Combines processed items, firing assembly transitions that consume two processed parts. |
| `task_painter_router` | Randomly selects items post-QC1 for painting or for direct packaging. |
| `task_quality_control` | Performs QC1 and QC2 (post-paint prioritized), tracks pass/fail probability, and returns tokens to workers or rework bins. |
| `task_reworker` | Reuses failed parts from the rework bin back into the processing stage. |
| `task_packager` | Packages individual items and triggers bulk packaging when five individual units are ready. |
| `task_monitor` | Periodically prints the token counts for every place to visualize the system state. |

## Building & Running

1. Open the `WIN32-MSVC` FreeRTOS demo solution in Visual Studio.
2. Build the `WIN32-MSVC` project which includes `main_blinky.c` as the demo entry point.
3. Run the compiled executable to observe the console-based manufacturing simulation.

The FreeRTOS scheduler is started after all worker tasks are created; the console output will show colored status messages for transitions, QC results, painting decisions, and the periodic monitor snapshot.