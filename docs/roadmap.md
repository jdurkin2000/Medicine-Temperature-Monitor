# Roadmap and known limitations

This page records current gaps. It is not blanket permission to address them
during an unrelated task.

## User features

- `ALARM`, `SCALE`, `VOLUME`, and `HSTRY` have no option editors.
- Alarm thresholds and the selected display unit are not persisted as user
  settings.
- History has no UI, timestamps, export, integrity check, or out-of-range
  duration summary.
- The display supports one decimal place and has limited range formatting.

## Sensor and data path

- The DS18B20 driver uses Skip ROM and assumes exactly one sensor.
- Conversion completion is not polled; firmware waits the configured worst-
  case interval.
- The one-hour read-reliability window is in RAM and restarts after reset.
- The path effectively represents temperature in 0.125 degC steps before
  rounding to 0.1 degC; it does not retain full 12-bit raw resolution.
- UART output is blocking, always Fahrenheit, and has no record delimiter.

## Architecture and maintainability

- Several names and the RTC ISR remain from TI's stopwatch demo.
- The history structure and storage helpers still live in
  `TempSensorMode.c` rather than a dedicated module.
- CCS-managed metadata and the vendored SDK make tooling changes sensitive to
  the configured TI environment.

## Validation and power

- There are no automated host tests.
- There are no hardware-in-the-loop tests.
- No repeatable bench-test report is checked in.
- There are no recorded EnergyTrace or current measurements.
- Battery life has not been calculated from measured state currents and an
  explicit battery model.
- The normal interval and development UART/LED behavior are not optimized for
  a production power target.

## Likely next milestones

These need separate design decisions before implementation:

1. Establish a repeatable baseline build and test checklist.
2. Measure current by operating state and populate the
   [power budget](power-budget.md).
3. Define production sampling and alarm-latency requirements.
4. Extract and version the FRAM storage format with corruption recovery.
5. Implement one settings editor end-to-end, including persistence and tests.
6. Define a timestamp source and data-export protocol.
7. Add host-testable pure logic for conversion, alarm state, and ring-buffer
   behavior.

Any milestone that changes hardware, user-visible cadence, or stored-data
semantics should begin with a design proposal and explicit approval.
