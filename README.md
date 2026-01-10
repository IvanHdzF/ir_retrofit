# Infrared Transceiver Platform

This repository contains an **ESP-IDF–based embedded platform** for developing and validating a **protocol-agnostic infrared (IR) control system**, with a long-term goal of a smart minisplit retrofit controller.

The project is structured to support:
- Multiple independent applications (demo, tests, experiments)
- Reusable system components and services
- Clear separation between system logic, middleware, and application code
- Strong documentation for design, requirements, and tradeoffs

This is a **systems-oriented repository**, not a single monolithic firmware.

---

## Repository Structure

```text
.
├─ apps/                 # Application entry points (selectable at build time)
│  ├─ infrared_test/     # IR bring-up and protocol experiments (NEC, RMT)
│  ├─ ble_test/          # BLE communication experiments
│  └─ system_demo/       # System integration demo (orchestrator + services)
│
├─ components/           # System-level reusable components (always compiled)
│  ├─ event_bus/
│  ├─ storage/
│  ├─ ir_service/
│  ├─ scheduler/
│  ├─ orchestrator/
│  ├─ auth/
│  └─ error_manager/
│
├─ middlewares/          # Optional, application-selected generic modules
│  ├─ ir_generic/
│  └─ rtc_generic/
│
├─ managed_components/   # ESP-IDF managed dependencies
│
├─ tests/
│  ├─ unit_tests/        # Host-based unit tests (logic, state machines)
│  └─ integration_tests/# Multi-module or hardware-assisted tests
│
├─ docs/                 # Architecture, requirements, and design documentation
│  ├─ DESIGN.md
│  ├─ SYSTEM_REQUIREMENTS.md
│  ├─ DESIGN_TRADEOFFS.md
│  ├─ DEVELOPMENT_PLAN.md
│  └─ ESP_IDF_MINI_FRAMEWORK.md
│
├─ main/                 # ESP-IDF default main (unused by apps, kept minimal)
├─ build/                # Build artifacts (not committed)
│
├─ CMakeLists.txt        # Top-level build + application selection
├─ sdkconfig             # ESP-IDF configuration
├─ README.md             # (this file)
└─ .gitignore
```

---

## Key Concepts

### Multiple Applications
Applications live under `apps/` and are selected at build time via CMake:

```bash
idf.py -DAPP_NAME=infrared_test build
```

Each application:
- Has its own `CMakeLists.txt`
- Provides its own `app_main()`
- Declares its ESP-IDF–specific dependencies

This enables clean bring-up, testing, and demos without polluting system code.

---

### System Components
Modules under `components/` implement **core system functionality**:
- Orchestrator (FSM and capability gating)
- Event Bus (async pub/sub)
- Storage Service (atomic persistence)
- Infrared Service (learn/send IR)
- Scheduler (time-based routines)
- Authentication and Error Management

These components are designed to be:
- Reusable
- Unit-testable
- Loosely coupled via events

---

### Middleware
Middleware modules are:
- Optional
- Selected per application
- Ideally ESP-IDF–agnostic

Examples:
- Generic IR waveform handling
- RTC abstractions

Middleware inclusion is controlled centrally by the top-level `CMakeLists.txt`.

---

## Documentation Entry Points

Start here depending on what you want to understand:

- **System overview & architecture**  
  → `docs/DESIGN.md`

- **Functional and non-functional requirements**  
  → `docs/SYSTEM_REQUIREMENTS.md`

- **Why specific design decisions were made**  
  → `docs/DESIGN_TRADEOFFS.md`

- **Planned development order and milestones**  
  → `docs/DEVELOPMENT_PLAN.md`

- **ESP-IDF mini-framework and build structure**  
  → `docs/ESP_IDF_MINI_FRAMEWORK.md`

Component-specific details live under:
```
docs/components/
```

---
## Repository Setup

This repository uses Git submodules for shared components and external
dependencies.

After cloning, initialize all submodules:

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

## Build & Development

### Select an Application
```bash
idf.py -DAPP_NAME=infrared_test build
idf.py -DAPP_NAME=system_demo flash monitor
```

### ESP-IDF Version
- ESP-IDF v5.x (recommended)
- Uses RMT v2 driver APIs

### Tooling
- ESP-IDF CMake build system
- FreeRTOS
- Unity/CMock for host-based testing
- draw.io for component diagrams
- Mermaid for system-level flows

---

## Project Status

- Active development
- IR RX/TX bring-up functional
- System architecture frozen
- Core services under implementation

This repository prioritizes **correctness, clarity, and extensibility** over feature completeness.

## CI
- Firmware is built using ESP-IDF in a container
- Unit tests run in Wokwi simulation via pytest-embedded
- Local CI is reproducible using `act`

### Run locally
```bash
act -j run-target --bind \
  --matrix '{"espidf_target":"esp32s3"}' \
  --env ACT=true \
  --secret-file .secrets
```