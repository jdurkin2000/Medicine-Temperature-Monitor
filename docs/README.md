# Documentation index

The root [README](../README.md) is the project entry point. This directory
separates detailed context so developers and coding agents can load only what
their task requires.

## Choose by task

| If you are changing... | Read first |
| --- | --- |
| Main loop, interrupts, state ownership, clocks, timers, or module boundaries | [Architecture and design](architecture-and-design.md) |
| Sleep modes, sampling cadence, peripheral duty cycle, or battery selection | [Power budget](power-budget.md), then [Architecture and design](architecture-and-design.md) |
| Pins, wiring, power rails, board connections, or external components | [Hardware](hardware.md) |
| One-wire communication, sensor power, retries, LCD, UART, or buzzer code | [Sensors and drivers](sensors-and-drivers.md) |
| Alarms, buttons, settings, units, or LCD-visible behavior | [Firmware behavior](firmware-behavior.md) |
| FRAM layout, history, resets, validity, timestamps, or export | [Data and persistence](data-and-persistence.md) |
| CCS/VS Code setup, building, flashing, UART, or tunable constants | [Development guide](development.md) |
| Agent prompts or deciding which context an agent needs | [Agent guide](agent-guide.md) |
| Planning the next feature or checking incomplete work | [Roadmap and known limitations](roadmap.md) |

## Suggested reading paths

New contributors should read the root [README](../README.md), this index, and
[Architecture and design](architecture-and-design.md). Hardware work should add
[Hardware](hardware.md); firmware feature work should add only the relevant
behavior, driver, or storage document.

Coding agents must also read [AGENTS.md](../AGENTS.md). The focused routing
instructions in [Agent guide](agent-guide.md) are designed to avoid loading the
entire documentation set for every task.

## Documentation ownership

Each fact should have one primary home. Other documents should link to it
instead of copying it. When a change affects multiple areas, update each
affected document in the same change and keep the root README at summary level.

