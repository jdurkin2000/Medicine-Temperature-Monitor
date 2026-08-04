# Data and persistence

## Current FRAM history

Every completed measurement is stored in a 64-entry FRAM ring buffer. Samples
use signed tenths of a degree Celsius, the application's canonical temperature
unit. An eight-byte bitmap identifies whether each entry represents a valid or
invalid measurement.

Invalid entries retain the last known good value. This preserves continuity
for a future history UI without allowing stale data to be mistaken for a new
valid reading.

The history structure contains:

- a magic value;
- the next write index;
- the retained entry count;
- 64 signed fixed-point temperature samples; and
- an eight-byte validity bitmap.

The structure and its helper functions currently live inside
`TempSensorMode.c`. `temperatureHistoryLog` uses TI's `PERSISTENT` pragma, so
the data survives an ordinary reset. A firmware download can erase or replace
it.

## Startup recovery

The current layout can migrate the previous history layout; migrated retained
entries are marked valid. At boot, the newest recovered valid temperature may
be displayed immediately, but it is deliberately treated as unconfirmed for
the current session and shown with `!` until a fresh CRC-validated sample
arrives.

## Integrity and reset semantics

Current guarantees:

- ordinary resets retain the ring buffer;
- validity is separate from the stored numeric value;
- writes advance through a bounded circular capacity;
- the communication-quality window does not persist.

Not yet implemented:

- timestamps or sample sequence numbers;
- a history display or export protocol;
- a checksum/CRC or transactional recovery for interrupted writes;
- persisted alarm thresholds or display settings;
- accumulated out-of-range duration;
- versioned schema metadata beyond the current migration handling.

## Design guidance for future storage work

Storage changes should explicitly define:

1. the binary layout and version;
2. atomicity across brownouts or resets;
3. how partially written data is detected;
4. migration and factory-reset behavior;
5. retention and overwrite rules;
6. timestamp source and clock-loss semantics;
7. write frequency and its power cost; and
8. how tests can inject corruption and reset points.

Keep internal temperatures in fixed-point Celsius and convert only for display
or export. Do not silently reinterpret old FRAM contents when a layout changes.

User-visible alarm and stale-value behavior is documented in
[Firmware behavior](firmware-behavior.md).
