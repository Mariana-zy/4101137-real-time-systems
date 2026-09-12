# RET — Timing Evidence Report

**Team:** `<Mariana Zuluaga Yepes>` · **Boards:** `<ESP32-C6>`

## 1. The system and its task set

| ID | Requirement |
|---|---|
| REQ-CTRL-01 | While the system is irrigating, the flow/pressure control loop shall execute every 10 ms, within a deadline of 10 ms. |
| REQ-SAFE-01 | When an overpressure condition is detected, the system shall close the valve and stop the pump within 5 ms. |
| REQ-SENS-01 | While the controller is operating, the system shall sample sensors every 1 ms with bounded jitter. |
| REQ-TEL-01 | While telemetry is enabled, the system shall send operational telemetry to the Hub; delayed delivery shall degrade observability but shall not affect safe irrigation control. |
| REQ-CON-01 | When an operator submits a console command, the system shall return a useful response before the command’s firm deadline; responses after that deadline are discarded. |

| Task | Req. | Type (H/F/S) | Period | Deadline | Measured C_i | How it was measured |
|---|---|---:|---:|---:|---:|---|
| Flow/pressure control loop | REQ-CTRL-01 | H | 10 ms | 10 ms | TBD | GPIO pulse around the complete loop; logic analyzer / Zephyr trace |
| Overpressure emergency stop | REQ-SAFE-01 | H | Event-driven | < 5 ms | TBD | GPIO at pressure-event detection and valve-close command; logic analyzer |
| Sensor sampling | REQ-SENS-01 | H | 1 ms (1 kHz) | TBD | TBD | GPIO pulse around sensor read; trace to calculate execution time and jitter |
| Telemetry to Hub | REQ-TEL-01 | S | TBD | TBD | TBD | Network timestamp/trace from enqueue to transmission |
| Command console | REQ-CON-01 | F | Aperiodic | TBD | TBD | Timestamp command reception and response completion |

> `C_i` must be the worst observed execution time under the stated test load, not an estimated value.

## 2. ADRs

### ADR-001 — Use Zephyr on ESP32-S3 for the real-time irrigation controller

**Context:** SoilSense Control must execute a hard flow/pressure loop every 10 ms, sample sensors at 1 kHz, and react to overpressure in less than 5 ms.

**Decision:** Implement the controller on ESP32-S3 using Zephyr threads, explicit task priorities, tracing, and GPIO instrumentation for timing verification.

**Justification (with numbers):** The system contains hard timing constraints of 10 ms (control loop), 1 ms (sensor sampling period), and <5 ms (emergency stop). Zephyr supports thread scheduling and tracing needed to measure and demonstrate compliance.

**Status:** Proposed — validate with measured WCET, jitter, and emergency-stop latency.

## 3. Evidence by week

### Week 2 — Superloop baseline (board: `<ESP32-S3 serial/nickname>`)

| Measurement | Result | Evidence |
|---|---:|---|
| Control-loop period | TBD | Analyzer capture / trace file |
| Control-loop jitter | TBD | Analyzer capture / trace file |
| Control-loop worst execution time | TBD | Analyzer capture / trace file |

**Reading:** Pending measurement. The superloop baseline is the comparison point for the threaded Zephyr implementation.

### Week 3 — S3 baseline and silicon comparison

| Platform / build | Control period | Worst jitter | Worst C_i | Evidence |
|---|---:|---:|---:|---|
| ESP32-S3 superloop | TBD | TBD | TBD | `<trace/capture>` |
| ESP32-S3 Zephyr threads | TBD | TBD | TBD | `<trace/capture>` |
| `<comparison platform, if used>` | TBD | TBD | TBD | `<trace/capture>` |

**Reading:** Pending measurement. Compare worst-case values, not only averages.

## 4. Schedulability analysis

Use measured values only:

`U = C_control / 10 ms + C_sampling / 1 ms + ...`

**Scheduling test:** `<RM / hyperbolic bound / EDF>`  
**Result:** Pending until measured `C_i` values and all task periods/deadlines are available.  
**Blocking B_i:** `0` unless shared mutexes are introduced; if so, measure or bound the longest critical section.

## 5. Functional safety

**Declared safe state:** valve closed and pump disabled.

**Watchdog strategy:** If the control loop stops refreshing the watchdog, the watchdog shall reset or force the output stage into the valve-closed/pump-disabled state.

**Evidence required:** trace showing (1) normal watchdog refresh, (2) intentional control-loop failure, and (3) valve-close action within the applicable safety deadline.
