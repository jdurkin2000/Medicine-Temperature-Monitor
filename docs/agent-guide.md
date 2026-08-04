# Coding-agent guide

## Minimum context policy

Do not load every document by default. Begin with:

1. [`AGENTS.md`](../AGENTS.md) for repository-specific working rules;
2. the root [`README.md`](../README.md) for project scope and status;
3. one or two focused documents selected from the routing table below; and
4. only the source modules involved in the requested behavior.

| Task area | Focused context |
| --- | --- |
| Architecture, main loop, ISR, timers, clocks | [Architecture and design](architecture-and-design.md) |
| Low power, sleep, cadence, battery life | [Power budget](power-budget.md) plus the low-power section of [Architecture and design](architecture-and-design.md) |
| Wiring, pins, board, electrical interface | [Hardware](hardware.md) |
| DS18B20, one-wire, LCD HAL, buzzer, UART | [Sensors and drivers](sensors-and-drivers.md) |
| Alarms, buttons, settings, display behavior | [Firmware behavior](firmware-behavior.md) |
| FRAM, history, reset recovery, export | [Data and persistence](data-and-persistence.md) |
| Build, flash, CCS, tunable constants | [Development guide](development.md) |
| Feature planning and incomplete behavior | [Roadmap](roadmap.md) |

For TI/CCS work in the configured environment, also follow these local vendor
instructions when they are relevant and available:

- `C:/ti/ccs2100/ccs/theia/resources/ai/CCS.md`
- `C:/ti/ccs2100/ccs/theia/resources/ai/sdks/MSP430WARE/AGENTS.md`
- `C:/ti/ccs2100/ccs/theia/resources/ai/boards/MSP-EXP430FR6989/AGENTS.md`

Inspect the implementation and `git status` before editing. Preserve unrelated
worktree changes. After firmware edits, run the CCS Debug build and report its
exact result. Hand off unexercised hardware behavior as a concrete bench test.

## Implementation prompt template

Use this template without copying unrelated documentation or source files into
the request:

```text
Read AGENTS.md, README.md, and [specific focused doc] first.

Task: [one observable outcome]

Current behavior:
- [what happens now]

Required behavior:
- [precise behavioral change]

Relevant area:
- [likely files, functions, pins, timers, or state if known]

Must preserve:
- [specific architecture or behavior invariant]
- [behavior that must not regress]

Out of scope:
- [nearby features or refactors not requested]

Acceptance criteria:
1. [observable or measurable result]
2. [edge case]
3. [power, timing, or persistence requirement if applicable]

Verification:
- Build the CCS Debug configuration and report errors and warnings.
- [specific bench test or static check]

Inspect the current code before editing, make the smallest coherent change,
preserve unrelated worktree changes, and update the focused documents affected
by the change. Ask a question only if a missing decision would materially
change the implementation.
```

Good prompts state desired external behavior and constraints. They may mention
known symbols, but should let the agent verify current definitions rather than
prescribing an unverified implementation.

## Prompt-writing guidance for another LLM

```text
Using the supplied project summary and focused documentation as the source of
truth, turn my request into one compact, implementation-ready prompt for
Codex. Output only that prompt. Include the observable outcome,
current-versus-required behavior, relevant files or symbols, applicable design
invariants, explicit non-goals, acceptance criteria, build verification, and
any necessary hardware bench test. Do not invent missing hardware facts. Ask
at most one clarification, and only if different answers would materially
change the design.

My request: [describe the change here]
```

## Context-efficiency rules

- Give one coherent task per prompt.
- Link the focused document instead of retelling the whole system.
- Include exact logs only when they are evidence for the task.
- Point to likely modules or symbols without pasting entire files.
- Define non-regression constraints and explicit non-goals.
- Prefer measurable acceptance criteria over subjective goals.
- State what hardware is available and what must be observed.
- Request one final build unless an intermediate build is needed to isolate a
  failure.
- Do not combine a feature with broad cleanup, renaming, or speculative
  optimization.

