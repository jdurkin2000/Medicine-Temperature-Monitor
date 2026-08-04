# Medicine Temperature Monitor

Firmware for a low-power medicine temperature monitor built around the Texas
Instruments MSP430FR6989 LaunchPad and an external DS18B20 sensor.

The current prototype measures temperature, displays it on the LaunchPad LCD,
stores recent readings in FRAM, and raises visible and audible alarms for an
unsafe temperature, a missing probe, repeated CRC failures, or sustained poor
sensor communication.

> [!IMPORTANT]
> This is a learning and portfolio project, not a validated medical device.
> The present sampling rate and several implementation choices favor prototype
> interaction over production battery life.

## Project goals

- Run for months or years by sleeping and power-gating peripherals whenever
  possible.
- Let the user configure temperature limits and other settings.
- Retain temperature history and out-of-range duration across power loss.
- Warn clearly without losing measurements or history.
- Recover predictably from resets and be thoroughly testable.
- Demonstrate mature embedded-system design and documentation.

## Current status

Implemented:

- DS18B20 sampling at 11-bit resolution with presence and scratchpad CRC checks
- up to three read attempts per measurement
- 2.0-8.0 degC default alarm range
- separate alarm causes for range, probe presence, CRC, and signal quality
- segmented-LCD temperature, alarm, and settings-overlay rendering
- interrupt-driven buttons with debounce and hold detection
- 64-entry persistent FRAM temperature history
- LPM3 waits and switched sensor power

Not yet implemented:

- settings editors and persistence
- history UI, timestamps, export, integrity protection, and duration summaries
- automated host or hardware-in-the-loop tests
- recorded energy measurements and a validated battery-life estimate

See [Roadmap and known limitations](docs/roadmap.md) for the complete list.

## Documentation map

Read only the documents relevant to the task:

| Topic | Document |
| --- | --- |
| System structure, execution model, timers, and design invariants | [Architecture and design](docs/architecture-and-design.md) |
| Energy model, measurement plan, and battery-life calculation | [Power budget](docs/power-budget.md) |
| Board, pin ownership, wiring, and electrical safety | [Hardware](docs/hardware.md) |
| DS18B20 behavior, retries, DriverLib, LCD, UART, and buzzer drivers | [Sensors and drivers](docs/sensors-and-drivers.md) |
| Monitoring, alarms, buttons, settings overlay, and display behavior | [Firmware behavior](docs/firmware-behavior.md) |
| FRAM history format, validity, recovery, and future storage work | [Data and persistence](docs/data-and-persistence.md) |
| Build, flash, UART, configuration symbols, and debugging | [Development guide](docs/development.md) |
| Context-routing guidance for coding agents | [Agent guide](docs/agent-guide.md) |
| Missing features and likely next work | [Roadmap and known limitations](docs/roadmap.md) |
| All documentation by audience and task | [Documentation index](docs/README.md) |

## Quick start

### VS Code

Open the repository root. Build with `Ctrl+Shift+B` and select
`CCS: Build Smart Monitor (Debug)`. To program the board, run
`CCS: Build and Flash Smart Monitor (Debug)` from **Tasks: Run Task**.

The tasks assume Code Composer Studio is installed at `C:/ti/ccs2100`.

### Code Composer Studio

Import `Smart Monitor` as an existing CCS managed-build project, select the
`Debug` configuration, and build before starting a debug session.

For setup details and common debugging traps, read the
[Development guide](docs/development.md).

## Repository layout

| Path | Purpose |
| --- | --- |
| `Smart Monitor/` | Application firmware and CCS project |
| `Smart Monitor/driverlib/` | Vendored TI MSP430FR5xx/6xx DriverLib |
| `Smart Monitor/targetConfigs/` | CCS/eZ-FET target configuration |
| `.vscode/` | IntelliSense and CCS build/flash tasks |
| `docs/` | Task-oriented project documentation |
| `AGENTS.md` | Repository-wide instructions for coding agents |

Generated files such as `Smart Monitor/main.pp` and everything under
`Smart Monitor/Debug/` are not source files.

## Contributing

Keep changes scoped, preserve unrelated worktree edits, and update the focused
document that owns any changed behavior, interface, hardware mapping, power
assumption, or limitation. Link between documents instead of duplicating
content. Firmware changes should finish with a CCS Debug build and, when the
behavior is hardware-dependent, a concrete bench-test procedure.
