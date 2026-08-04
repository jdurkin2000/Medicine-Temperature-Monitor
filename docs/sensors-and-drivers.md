# Sensors and drivers

## DS18B20 measurement lifecycle

The monitoring loop performs this sequence for each measurement:

1. Enable the switched sensor supply on P8.4.
2. Wait 1 ms for the rail to settle.
3. Apply the volatile 11-bit resolution configuration and start conversion.
4. Sleep in LPM3 for the 375 ms maximum conversion time.
5. Read all nine scratchpad bytes and validate presence and CRC.
6. Retry a failed read up to two times against the completed conversion.
7. Disable the sensor supply after all read attempts finish.

At 11-bit resolution, the raw conversion step is 0.125 degC. Valid values are
rounded into signed tenths of a degree Celsius for the application. The driver
uses Skip ROM and therefore assumes exactly one device on the one-wire bus.

The driver waits the fixed worst-case conversion time; it does not poll the
sensor for early completion.

## Timing and interrupt behavior

`ds18b20.c` uses P1.4 for data, P8.4 for sensor-power control, and Timer_A2
with the 1 MHz SMCLK for microsecond delays. One-wire bit slots are timing-
sensitive, so reset/read/write transactions briefly mask interrupts.

The long conversion delay is not timing-critical at the bit level. It runs
through Timer_A3 and LPM3, allowing button and display-related interrupts to
remain responsive.

The DQ pull-up and sensor VDD must be connected to the same switched rail.
See [Hardware](hardware.md) for the recommended circuit and reset-state
pull-down.

## Validation, retry, and failure classification

Each raw attempt contributes to the communication-quality counters. A valid
result requires both sensor presence and a matching scratchpad CRC.

- If conversion never starts, or the final read retry loses presence, the
  result is classified as **no presence**.
- If conversion starts and the final failed read has presence but a CRC
  mismatch, the result is classified as **CRC mismatch**.
- Mixed failures are classified by the final attempt.
- A later valid sample clears both live communication conditions.

After all three attempts fail, the firmware records an invalid history entry
containing the last known good value. It does not transmit a UART temperature
for that measurement. If there has never been a valid value, the display shows
`FAULT`; otherwise it retains the stale numeric value.

## Communication-quality window

The firmware counts successful and total raw attempts in 60 one-minute RAM
buckets. After a complete 60-minute warm-up, a success rate below 90% activates
the signal-degradation condition. Exactly 90% is accepted. The window restarts
after reset and is not stored in FRAM.

Alarm latching and acknowledgement rules are described in
[Firmware behavior](firmware-behavior.md).

## Other driver boundaries

### LCD

`hal_LCD.c/.h` initializes LCD_C and owns segment character maps, LCD positions,
and basic clear/write operations. Settings and alarm scrolling are scheduled
nonblockingly outside the HAL. A legacy blocking scroll helper remains but is
not the architecture for active overlays.

### Buzzer and LEDs

Timer_A1 generates the passive-buzzer PWM on P1.3. Alarm and button-feedback
requests share the timer through `updateBuzzerOutput()` arbitration. Timer_B0
ends the short button-feedback tone. The red and green LEDs are direct active-
high GPIO outputs.

### UART

`debug.c/.h` provides polling, blocking transmit helpers over UCA1. The
monitoring path currently sends Fahrenheit for each valid sample. Blocking
UART and float formatting are development conveniences and power-budget
hotspots, not a production data-export protocol.

### TI DriverLib

`Smart Monitor/driverlib/` is vendored TI MSP430FR5xx/6xx DriverLib. Do not
modify it for application features. Wrap or compose its interfaces from the
application modules instead.

## Driver change checklist

When changing a driver:

- verify pin and timer ownership against
  [Architecture and design](architecture-and-design.md);
- keep long delays sleep-based rather than busy-waiting;
- document any interval during which interrupts are masked;
- leave peripherals in a defined low-leakage state when powered down;
- validate presence, integrity, timeout, and retry behavior;
- build with the TI compiler; and
- provide a bench test for behavior that cannot be exercised locally.

