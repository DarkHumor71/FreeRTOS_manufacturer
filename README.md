# FreeRTOS Manufacturing Process Control Demo

A real-time manufacturing process control system implemented using **FreeRTOS** with a **Petri net** model of computation. This demo simulates a complete production line with material processing, assembly, quality control, optional painting, packaging, and rework capabilities—all running on the FreeRTOS Windows MSVC port.

---

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
  - [Petri Net Model](#petri-net-model)
  - [System Components](#system-components)
- [Building and Running](#building-and-running)
  - [Prerequisites](#prerequisites)
  - [Build Instructions](#build-instructions)
  - [Running the Demo](#running-the-demo)
- [Status Viewer (Web UI)](#status-viewer-web-ui)
  - [Starting the Viewer](#starting-the-viewer)
  - [Network Access](#network-access)
- [System Details](#system-details)
  - [Places (States)](#places-states)
  - [Transitions (Events)](#transitions-events)
  - [FreeRTOS Tasks](#freertos-tasks)
- [Petri Net Visualization](#petri-net-visualization)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Overview

This demo models a realistic manufacturing production line using **Petri nets**, a mathematical framework for modeling distributed systems. The system demonstrates:

- **Concurrent task execution** using FreeRTOS
- **Resource constraints** (limited QC workers shared across QC1, QC2, and rework operations)
- **Decision logic** (random paint selection)
- **Quality control** with pass/fail and rework loops
- **Bulk packaging** (batching 5 individual units)
- **Real-time monitoring** via HTTP JSON API

The manufacturing line processes raw materials through multiple stages:

```
Raw Material → Processing → Assembly → QC1 → Paint/Skip → QC2 → Packaging → Shipment
                                              ↓
                                         Rework Bin → Back to Processed
```

---

## Features

✅ **Petri Net Model of Computation** - Places, transitions, and tokens represent system state and flow  
✅ **15 Places** - Representing buffers, states, and resources  
✅ **16 Transitions** - Representing manufacturing operations and decisions  
✅ **Thread-Safe Operations** - Mutexes protect shared Petri net state  
✅ **Colored Console Output** - ANSI color-coded task logs for clarity  
✅ **HTTP Status Server** - JSON endpoint for real-time system monitoring  
✅ **Web-Based Status Viewer** - Live table showing token counts per place  
✅ **Network Support** - Access status from any device on your network  
✅ **Rework Loop** - Failed QC items re-enter the processing pipeline  
✅ **Randomized Behavior** - Paint selection and QC pass/fail probabilities  
✅ **Interactive Controls** - Keyboard input to increase raw materials during runtime  

---

## Architecture

### Petri Net Model

The system uses a **Petri net** to model the manufacturing process:

- **Places**: States or buffers (e.g., "Raw Material", "Assembled", "Worker")
- **Transitions**: Events or operations (e.g., "Start Processing", "Pass QC")
- **Tokens**: Workpieces or resources flowing through the system
- **Arcs**: Directed edges showing input/output relationships

**Key Properties:**
- **Mutex-protected token counts** ensure thread-safe state updates
- **Global net mutex** serializes transition firing to prevent race conditions
- **Weighted arcs** model resource consumption (e.g., assembly requires 2 processed items)

### System Components

| Component | Description |
|-----------|-------------|
| **Petri Net Engine** | Core logic for enabling and firing transitions |
| **FreeRTOS Scheduler** | Manages concurrent tasks for each manufacturing station |
| **Console Logger** | Thread-safe, colored output for task events |
| **HTTP Status Server** | Serves JSON representation of current system state |
| **Web Viewer** | React-based UI polling the status endpoint |

---

## Building and Running

### Prerequisites

- **Visual Studio 2019 or later** (with MSVC toolchain)
- **Windows 10/11** (for console color support)
- **FreeRTOS Windows Port** (included in this repository)
- *Optional:* **Node.js** or **Python 3** (for serving the web viewer)

### Build Instructions

1. **Open the Solution**
   ```
   Open WIN32-MSVC.sln in Visual Studio
   ```

2. **Select Configuration**
   - Debug or Release (both work)
   - Platform: x64 or Win32

3. **Build the Project**
   - Build → Build Solution (Ctrl+Shift+B)
   - The executable will be in `Debug/` or `Release/` directory

### Running the Demo

1. **Start the Executable**
   - Press F5 (Debug) or Ctrl+F5 (Release) in Visual Studio
   - Or run `WIN32-MSVC.exe` directly from the output folder
   - run npx serve satus-viewer

2. **Observe Console Output**
   - You'll see colored logs for each task:
     - **Cyan**: Material Loader
     - **Blue**: Processor, Packager
     - **Magenta**: Assembler, Paint Router
     - **Yellow**: QC Worker (in-progress)
     - **Green**: QC Pass, Bulk Package
     - **Red**: QC Fail

3. **Interactive Controls**
   - Press **`+`** key to increase raw materials by 10 units
   - This allows you to add more materials to the production line as needed

4. **Example Console Output**
   ```
   ===========================================================
   |   MANUFACTURING PROCESS CONTROL SYSTEM                    |
   |   Using FreeRTOS with Petri Net Model of Computation      |
   |   Running on Windows MSVC Port                            |
   ===========================================================

   System initialized with 20 raw materials
   Starting manufacturing tasks...

   [Material Loader] Loaded raw material -> Ready to Process
   [Processor] Started processing item #1
   [Processor] Finished processing item #1
   [Assembler] Started assembly #1 (combining 2 processed items)
   ...
   ```

---

## Status Viewer (Web UI)

A lightweight web page provides real-time visualization of the Petri net state.

### Starting the Viewer

The viewer is located in the `status-viewer/` directory.

**Option 1: Using Node.js (Recommended)**
```bash
cd status-viewer
npx serve
```
Then open your browser to the displayed URL (usually `http://localhost:5000`).

**Option 2: Using Python**
```bash
cd status-viewer
python -m http.server 3000
```
Then navigate to `http://localhost:3000`.

**Troubleshooting:**
- If you get 404 errors, try running the server from inside the `status-viewer` directory
- You can also access the file directly: `http://localhost:PORT/index.html`
- Make sure the FreeRTOS demo is running and serving on port 8080

### What You'll See

A live-updating table showing:
- **Place Name** (e.g., "Raw Material", "Assembled")
- **Token Count** (current number of items/resources in that place)

The table updates every second by polling `http://localhost:8080/` (the JSON status server).

### Network Access

**Accessing from Other Devices:**

1. **Find Your Computer's IP Address**
   - Windows: `ipconfig` in Command Prompt
   - Look for "IPv4 Address" (e.g., `192.168.1.100`)

2. **Ensure Firewall Allows Port 8080**
   - The FreeRTOS demo binds to `INADDR_ANY` (all network interfaces)
   - You may need to allow incoming connections on port 8080

3. **Serve the Viewer on Your Network**
   - **Python:** `python -m http.server 3000 --bind 0.0.0.0`
   - **Node.js:** `npx serve` (already serves on all interfaces)

4. **Access from Another Device**
   - Status API: `http://192.168.1.100:8080/`
   - Viewer: `http://192.168.1.100:3000/` (or port 5000 for Node.js)

---

## System Details

### Places (States)

| ID | Name | Initial Tokens | Description |
|----|------|----------------|-------------|
| P0 | Raw Material | 20 | Unprocessed materials waiting to enter the line |
| P1 | Ready to Process | 0 | Materials queued for processing |
| P2 | Processing | 0 | Materials currently being processed |
| P3 | Processed | 0 | Finished processed items |
| P4 | Ready to Assemble | 0 | Items queued for assembly |
| P5 | Assembled | 0 | Completed assemblies |
| P6 | QC Active 1 | 0 | Items undergoing first quality check |
| P7 | Passed QC1 / Decision | 0 | Items awaiting paint decision |
| P8 | Ready for Individual Package | 0 | Items ready to be packaged |
| P9 | Individually Packaged | 0 | Single-unit packaged items |
| P10 | Final Packaged | 0 | Bulk packages ready for shipment |
| P11 | Painted | 0 | Items with custom paint awaiting QC2 |
| P12 | QC Active 2 | 0 | Painted items undergoing quality check |
| P13 | Worker | 1 | QC worker resource token (limits concurrent QC) |
| P14 | Rework Bin | 0 | Failed items awaiting rework |

### Transitions (Events)

| ID | Name | Input Places | Output Places | Description |
|----|------|--------------|---------------|-------------|
| T0 | Load Material | P0 (1) | P1 (1) | Load raw material into processing queue |
| T1 | Start Processing | P1 (1) | P2 (1) | Begin processing operation |
| T2 | Finish Processing | P2 (1) | P3 (1) | Complete processing |
| T3 | Start Assembly | P3 (2) | P4 (1) | Combine 2 processed items |
| T4 | Finish Assembly | P4 (1) | P5 (1) | Complete assembly |
| T5 | Start QC 1 | P5 (1), P13 (1) | P6 (1) | Begin first quality check (requires worker) |
| T6 | Pass QC 1 | P6 (1) | P7 (1), P13 (1) | Pass first QC, release worker |
| T7 | Fail QC 1 | P6 (1) | P14 (1), P13 (1) | Fail first QC, send to rework, release worker |
| T8 | Select to Paint | P7 (1) | P11 (1) | Choose item for painting (30% chance) |
| T9 | Skip Paint | P7 (1) | P8 (1) | Skip painting, go to packaging |
| T10 | Start QC 2 | P11 (1), P13 (1) | P12 (1) | Begin second QC for painted items |
| T11 | Pass QC 2 | P12 (1) | P8 (1), P13 (1) | Pass second QC |
| T12 | Fail QC 2 | P12 (1) | P14 (1), P13 (1) | Fail second QC |
| T13 | Individual Package | P8 (1) | P9 (1) | Package single unit |
| T14 | Bulk Package | P9 (5) | P10 (1) | Combine 5 units into bulk package |
| T15 | Rework Process | P14 (1), P13 (1) | P3 (1), P13 (1) | Rework failed item back to processed state (requires worker) |

### FreeRTOS Tasks

| Task Name | Priority | Stack Size | Responsibility |
|-----------|----------|------------|----------------|
| `task_material_loader` | 3 | 256 words | Feeds raw materials every 800ms |
| `task_processor` | 3 | 256 words | Processes items (1.5s simulation delay) |
| `task_assembler` | 3 | 256 words | Assembles 2 processed items (1.2s delay) |
| `task_painter_router` | 3 | 256 words | Decides paint/skip with 30% paint probability |
| `task_quality_control` | 4 | 256 words | Performs QC1 and QC2 (5% fail rate) |
| `task_packager` | 3 | 256 words | Packages individual and bulk units |
| `task_reworker` | 2 | 256 words | Processes rework bin items (2.5s delay) |
| `task_status_server` | 2 | 384 words | Serves HTTP JSON status on port 8080 |

---

## Petri Net Visualization

The included `PIPE.pnml` file can be opened in **PIPE (Platform Independent Petri net Editor)** or similar tools for graphical visualization and analysis.

**Using PIPE:**
1. Download PIPE from [https://pipe2.sourceforge.net/](https://pipe2.sourceforge.net/)
2. Open `PIPE.pnml` in the editor
3. Visualize places, transitions, and arcs
4. Perform reachability analysis, invariant checking, etc.

**PNML Format:**
The Petri net definition exactly matches the C code implementation, including:
- 15 places with initial markings
- 16 transitions
- Weighted arcs representing token flow

---

## Configuration

### Modifying System Behavior

**Change Initial Raw Materials:**
```c
add_place("Raw Material", 20);  // Change 20 to desired count
```

**Adjust Paint Probability:**
```c
const int paint_chance_percent = 30;  // Change from 30% to desired value
```

**Modify QC Fail Rate:**
```c
const int fail_chance_percent = 5;  // Change from 5% to desired value
```

**Change Bulk Package Size:**
```c
add_arc_input(T_BULK_PACKAGE, P_INDIVIDUALLY_PACKAGED, 5);  // Change 5 to desired batch size
```

**Adjust Task Timing:**
- Material loader: `pdMS_TO_TICKS(800)` → change 800ms
- Processor: `pdMS_TO_TICKS(1500)` → change 1500ms processing time
- Assembler: `pdMS_TO_TICKS(1200)` → change 1200ms assembly time

### Network Configuration

**Change Status Server Port:**
```c
#define STATUS_SERVER_PORT 8080  // Change to desired port
```

**Modify Buffer Sizes:**
```c
#define STATUS_JSON_BUFFER 2048  // Increase if JSON payload is truncated
```

---

## Troubleshooting

### Common Issues

**Console colors not working:**
- Ensure you're running on Windows 10/11
- The demo enables ANSI color support programmatically
- Use Windows Terminal for best results

**Status server not accessible:**
- Check firewall settings (allow port 8080)
- Verify the demo is running (status server starts automatically)
- Test locally: `http://localhost:8080/` in a browser

**Web viewer shows old data:**
- The viewer polls every 1 second
- Check browser console for CORS or fetch errors
- Ensure the demo is still running

**Build errors:**
- Ensure FreeRTOS headers are in the include path
- Verify MSVC toolchain is installed (C99 or later required)
- Check that `ws2_32.lib` (Winsock library) is linked

**Tasks not executing:**
- Check for mutex deadlocks (shouldn't happen with correct code)
- Verify FreeRTOS scheduler started successfully
- Increase heap size in `FreeRTOSConfig.h` if needed

### Debug Output

If you encounter issues, check the console output for:
- `ERROR: Failed to create [X] mutex` → Insufficient heap memory
- `ERROR: Failed to create [X] task` → Stack or priority issues
- `Scheduler failed to start` → Heap memory exhausted

---

## License

This demo is provided as-is for educational and demonstration purposes. FreeRTOS is distributed under the MIT License. See the FreeRTOS repository for full licensing details.

---

## Credits

- **FreeRTOS**: Real-time operating system kernel
- **Petri Net Theory**: Carl Adam Petri
- **Implementation**: FreeRTOS Windows MSVC Port Demo

---

## Further Reading

- [FreeRTOS Official Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [Petri Nets Introduction](https://en.wikipedia.org/wiki/Petri_net)
- [PIPE Petri Net Editor](https://pipe2.sourceforge.net/)

---

START: 20 Raw Materials (P0)

↓ T0: Load (every 800ms)
P1: Ready to Process

↓ T1: Start Processing
P2: Processing (1500ms)
↓ T2: Finish Processing
P3: Processed

↓ T3: Start Assembly [needs 2!]
P4: Ready to Assemble
↓ T4: Finish Assembly (1200ms)
P5: Assembled → Now 10 units max

↓ T5: Start QC1 [needs Worker!]
P6: QC Active 1 (1000ms)
↓
├─ T6: Pass (95%) → P7: Decision Point
│                    ↓
│                    ├─ T8: Paint (30%) → P11: Painted
│                    │                     ↓ T10: Start QC2
│                    │                     P12: QC Active 2
│                    │                     ↓
│                    │                     ├─ T11: Pass → P8
│                    │                     └─ T12: Fail → P14
│                    │
│                    └─ T9: Skip (70%) → P8: Ready for Package
│
└─ T7: Fail (5%) → P14: Rework Bin
                    ↓ T15: Rework (2500ms)
                    Back to P3 (loop!)

P8: Ready for Individual Package
↓ T13: Individual Package
P9: Individually Packaged

↓ T14: Bulk Package [needs 5!]
P10: Final Packaged ← SHIPPING!

use https://www.fernuni-hagen.de/ilovepetrinets/fapra/wise23/rot/index.html to visualize