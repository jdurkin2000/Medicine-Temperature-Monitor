# Hardware

## Platform and toolchain

| Item | Current platform |
| --- | --- |
| Board | TI MSP-EXP430FR6989 LaunchPad |
| MCU | MSP430FR6989 16-bit FRAM microcontroller |
| IDE | Code Composer Studio 21.0.0 on Windows 11 |
| Compiler | TI MSP430 Code Generation Tools 21.6.2 LTS |
| SDK origin | MSP430Ware 3.80.14.01 LaunchPad example |
| Display | Onboard FH-1138P six-character segmented LCD |
| Debug/programming | Onboard eZ-FET over Micro-USB using Spy-Bi-Wire |

The repository includes MSP430FR5xx/6xx DriverLib under
`Smart Monitor/driverlib/`; this is not a SysConfig project.

## Pin and peripheral ownership

| Function | MSP430 resource | LaunchPad connection | Notes |
| --- | --- | --- | --- |
| DS18B20 data | P1.4 | J2.7 | One-wire bus; 4.7 kohm pull-up to switched sensor rail |
| DS18B20 power enable | P8.4 | J1.8 | Active high; preferably drives a load-switch enable |
| Passive buzzer PWM | P1.3 / TA1.2 | J4.1 | Approximately 2.048 kHz, active-high PWM |
| Red alarm LED | P1.0 | Onboard LED1 | Active high |
| Green measurement LED | P9.7 | Onboard LED2 | Active high |
| S1 | P1.1 | Onboard S1 / J1.6 | Active low, internal pull-up |
| S2 | P1.2 | Onboard S2 / J1.5 | Active low, internal pull-up |
| UART TX | P3.4 / UCA1TXD | eZ-FET backchannel | J101 TXD jumper required |
| UART RX | P3.5 / UCA1RXD | eZ-FET backchannel | Configured but unread; J101 RXD jumper required |
| LCD | LCD_C segment pins | Onboard LCD | Driven by `hal_LCD.c` |
| Low-frequency crystal | PJ.4/PJ.5 | Onboard 32.768 kHz LFXT | Supplies ACLK in LPM3 |

Timer ownership is documented in
[Architecture and design](architecture-and-design.md).

## DS18B20 switched-supply circuit

For a product-quality design, connect P8.4 to the active-high enable of a
low-leakage 3.3 V load switch. Connect the switch output to both DS18B20 VDD
and the top of the 4.7 kohm DQ pull-up. Place a 0.1 uF ceramic decoupling
capacitor close to the sensor.

Choose a switch with an enable pull-down or add one externally, for example
100 kohm, so the sensor remains off while MCU pins are high-impedance during
reset. Powering this low-current prototype directly from P8.4 is firmware-
compatible, but a load switch provides better voltage margin and isolates
sensor inrush from the GPIO.

Do not connect the DQ pull-up to the always-on 3.3 V rail. The DS18B20 can be
parasite-powered through DQ, which would defeat power gating and corrupt the
power budget.

## Buzzer drive

A small passive piezo may be connected between P1.3 and ground only when its
current remains within the GPIO rating. Use a correctly biased transistor
driver with a common ground for a buzzer or module that could exceed that
rating. Never power an unknown buzzer load directly from an MCU GPIO.

## Power and bench safety

Use only one source for each rail. Do not connect an external regulated 3.3 V
supply in parallel with the eZ-FET regulator. Remove the J101 3V3 isolation
jumper before externally powering the target 3.3 V rail unless the power
arrangement is intentionally designed to prevent backfeeding.

Always establish a common ground, verify voltage and polarity with power off,
and check for shorts before reconnecting USB. Recheck the LaunchPad user guide
before changing isolation jumpers or measuring current.

## Primary references

- [MSP-EXP430FR6989 LaunchPad Development Kit User's Guide](https://www.ti.com/lit/pdf/slau627)
- [MSP430FR58xx/59xx/68xx/69xx Family User's Guide](https://www.ti.com/lit/pdf/slau367)
- [MSP430FR6989 product page and datasheet](https://www.ti.com/product/MSP430FR6989)
- [MSP430FR6989 errata, including PMM32](https://www.ti.com/lit/pdf/slaz517)
- [DS18B20 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf)
