# Power budget

## Status

The firmware uses important low-power techniques, but the project does not yet
have recorded EnergyTrace or bench-current measurements. Therefore it does not
yet have a defensible average-current or battery-life claim.

This document is the source of truth for power assumptions, measurements, and
targets. Populate measured rows before using the budget to choose a battery or
claim an operating lifetime.

## Current power strategy

- Long waits use LPM3 with ACLK sourced from the 32.768 kHz crystal.
- The DS18B20 and its DQ pull-up are power-gated together on P8.4.
- The sensor runs at 11-bit resolution, limiting the worst-case conversion wait
  to 375 ms.
- A Timer_A3 interrupt wakes the CPU rather than an active busy wait during
  conversion and between samples.
- The green LED is limited to about 50 ms per normal measurement.
- Alarm output is duty-cycled: about 100 ms on followed by 650 ms off.

See [Architecture and design](architecture-and-design.md) for the PMM32-safe
LPM3 entry path and timer ownership. See
[Sensors and drivers](sensors-and-drivers.md) for sensor power switching.

## Prototype duty cycle

Normal monitoring currently consists of approximately:

1. 1 ms sensor-rail settling;
2. one or more short one-wire command/read transactions;
3. 375 ms sensor conversion, mostly in LPM3;
4. result processing, FRAM logging, LCD update, and blocking UART output;
5. an additional 1 s LPM3 delay.

The nominal cadence is about one sample every 1.375 seconds, excluding
transaction and processing time. This deliberately short interval is useful
for development but unsuitable as the basis of a years-long battery target.
Alarm mode removes the extra normal delay and samples again after each
conversion.

## Budget model

For each operating state, measure current and duration. Average current is:

```text
I_average = sum(I_state * t_state) / observation_time
```

An initial idealized battery-life estimate is:

```text
life_hours = usable_capacity_mAh / I_average_mA
```

Then derate usable capacity for battery self-discharge, temperature, regulator
quiescent current and efficiency, pulse-load behavior, end-of-life voltage,
sensor/load-switch leakage, and the expected alarm/interaction duty cycle.

## Measurement worksheet

Do not replace `TBD` with datasheet maxima when a board-level measured value is
needed. Datasheet figures may be recorded separately as design bounds.

| State | Expected activity | Current | Duration/frequency | Evidence |
| --- | --- | ---: | ---: | --- |
| Deep idle | MCU in LPM3, LCD active, sensor rail off | TBD | Between samples | EnergyTrace/ammeter |
| Sensor conversion | DS18B20 powered, MCU mostly LPM3 | TBD | About 375 ms/sample | EnergyTrace/ammeter |
| One-wire active | MCU active at 1 MHz, Timer_A2 timing | TBD | Per transaction | EnergyTrace capture |
| Result processing | Math, alarm evaluation, LCD, FRAM | TBD | Per sample | EnergyTrace capture |
| UART transmit | Blocking transmit through eZ-FET path | TBD | Valid samples | EnergyTrace capture |
| Green LED | P9.7 LED on | TBD | About 50 ms/normal sample | Current delta |
| Alarm pulse | Red LED and buzzer PWM active | TBD | 100 ms every 750 ms | Current delta |
| Button/settings | Debounce timer and LCD interaction | TBD | User-dependent | Scenario measurement |
| Board static load | LaunchPad debugger/regulators/jumpers | TBD | Continuous on prototype | Board-vs-target comparison |

## Recommended measurement procedure

1. Build a known firmware revision and record its commit identifier and
   configuration constants.
2. Measure a normal monitoring window long enough to include at least 20
   samples.
3. Mark or isolate idle, conversion, processing/UART, and LED phases.
4. Repeat with the sensor absent, a CRC-failure test setup if available, and an
   active alarm.
5. Separate LaunchPad/debugger overhead from target current before projecting
   a product battery life.
6. Record supply voltage, instrument, sample rate, ambient temperature, board
   wiring, and whether USB/eZ-FET was connected.
7. Store the results in this table or link a versioned report from here.

## Optimization priorities

These are candidates, not pre-approved implementation tasks:

1. Increase the normal sample interval based on medicine-monitoring response
   requirements.
2. Remove routine UART output or buffer it for explicit data-download sessions.
3. Replace floating-point presentation math where measurement shows meaningful
   savings.
4. Reduce or disable development LEDs in a production configuration.
5. Measure LCD, regulator, load-switch, pull-up, and board leakage before
   optimizing CPU instructions.
6. Evaluate a bare target board; the LaunchPad and debugger can dominate a
   low-current system measurement.

Any optimization must preserve sensor reliability, alarm latency, data
integrity, and the architecture invariants. Ask before implementing a power
optimization that changes user-visible cadence or hardware.

