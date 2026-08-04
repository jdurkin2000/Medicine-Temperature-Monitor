# Firmware behavior

## Normal monitoring

In normal operation the firmware samples at a prototype cadence of about
1.375 seconds: a 375 ms DS18B20 conversion followed by an additional one-
second delay, plus transaction and processing time.

For each valid result it:

- rounds the temperature to tenths of a degree Celsius;
- appends a valid FRAM history entry;
- transmits Fahrenheit through the UART backchannel;
- checks the range and communication alarm conditions; and
- refreshes the LCD unless a settings or alarm-message overlay owns it.

For an invalid result after three attempts, it appends an invalid entry using
the last known good value and does not send a UART temperature.

The green measurement LED turns on for 50 ms at the start of normal sampling.
The LCD displays one decimal place with a fixed degree symbol and `C` or `F`.
S2 changes the unit immediately without waiting for another measurement.

Sensor sequencing and failure classification are detailed in
[Sensors and drivers](sensors-and-drivers.md).

## Alarm causes

The default safe range is inclusive: 2.0 through 8.0 degC. Four independent
conditions feed the shared alarm output:

| Cause | Live condition | LCD message |
| --- | --- | --- |
| High/low temperature | Valid sample outside inclusive range | `TEMP HI` or `TEMP LO` |
| Probe absence | Final sensor attempt has no presence | `NO PROBE` |
| CRC mismatch | Final attempt has presence but fails CRC | `BAD CRC` |
| Signal degradation | Complete one-hour window is below 90% successful | `WEAK SIGNAL` |

When any cause activates:

- the green LED stays off;
- the red LED and passive-buzzer tone pulse for 100 ms;
- the pulse is followed by 650 ms off;
- the LCD exclamation icon stays enabled;
- the settings overlay closes;
- active-cause messages scroll at 200 ms per step; and
- sampling repeats immediately after each 375 ms conversion, without the
  normal extra delay.

The active-cause set refreshes after each complete message loop. If all live
conditions clear before the latched alarm is acknowledged, the most recent
cause cycle remains visible.

## Alarm acknowledgement and rearming

Pressing either S1 or S2 acknowledges the alarm. The physical press is
consumed, so it cannot also change a setting or display unit.

Each cause has independent acknowledgement and rearm state. After
acknowledgement it remains disarmed while its condition is still present. A
later cleared condition rearms that cause. For temperature range, this means
one later in-range sample is required. A later successful sensor result clears
the presence and CRC conditions.

## Buttons

Both LaunchPad buttons are active-low and use interrupt-driven debounce and
hold detection.

| Context | S1 | S2 |
| --- | --- | --- |
| Normal display, short press | Feedback beep only | Toggle C/F immediately |
| Normal display, 1 s hold | Enter settings | No long-hold action |
| Settings, short press | Select option (currently no-op) | Next option |
| Settings, 1 s hold | Exit settings | Exit settings |
| Active alarm, any press | Acknowledge and consume press | Acknowledge and consume press |

## Settings overlay

Entering settings scrolls `SETTINGS` across the six-character LCD at 200 ms
per step, then shows one of `ALARM`, `SCALE`, `VOLUME`, or `HSTRY`. `HSTRY` is
the six-character label for `HISTORY`.

The settings UI is an LCD overlay. Temperature conversion, alarm evaluation,
UART output, and FRAM logging continue behind it. The option editors are not
implemented; `settingsModeSelectOption()` intentionally does nothing.

## Display ownership and stale data

The temperature display is suppressed while an overlay owns the LCD. A valid
fresh sample clears the stale marker. At boot, a last-good value recovered from
FRAM is shown immediately with `!` because it is not confirmed for the new
session. The marker clears only after a fresh CRC-validated sample.

If sensor communication fails after a good sample, the numeric value remains
as stale history while the relevant alarm message is available. If no good
sample exists, the numeric display reads `FAULT`.

