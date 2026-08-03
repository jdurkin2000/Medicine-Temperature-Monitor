// Taken from https://www.embeddedrelated.com/showcode/294.php

#include <driverlib.h>
#include "ds18b20.h"

#define ONE_WIRE_PIN BIT4
#define ONE_WIRE_IN P1IN
#define ONE_WIRE_OUT P1OUT
#define ONE_WIRE_DIR P1DIR

#define DS18B20_RESET_LOW_CYCLES 500U
#define DS18B20_PRESENCE_SAMPLE_CYCLES 70U
#define DS18B20_RESET_RECOVERY_CYCLES 430U
#define DS18B20_READ_INIT_CYCLES 2U
#define DS18B20_READ_SAMPLE_CYCLES 8U
#define DS18B20_READ_REMAINDER_CYCLES 52U
#define DS18B20_SCRATCHPAD_BYTES 9U
#define DS18B20_CRC_DATA_BYTES 8U

static uint8_t read_18B20_byte(void);
static uint8_t crc8_update(uint8_t crc, uint8_t byte);

/*
 * Busy-wait for the requested number of microseconds.
 *
 * Init_Clock() configures SMCLK to 1 MHz, so each Timer A2 tick is 1 us.
 * Timer A2 is reserved for this delay and must not be used elsewhere.
 */
void delay_us(unsigned long microseconds)
{
    while (microseconds != 0UL)
    {
        unsigned int interval;
        unsigned int start;

        interval = (microseconds > 0xFFFFUL)
                       ? 0xFFFFU
                       : (unsigned int)microseconds;

        TA2CTL = TASSEL__SMCLK | MC__CONTINUOUS | TACLR;
        start = TA2R;

        while ((unsigned int)(TA2R - start) < interval)
        {
            /* Busy-wait while Timer A2 measures the elapsed time. */
        }

        TA2CTL = MC__STOP;
        microseconds -= interval;
    }
}

Ds18b20Status ds18b20_start_conversion(void)
{
    unsigned int interruptState = __get_SR_register() & GIE;
    Ds18b20Status status = DS18B20_STATUS_NO_PRESENCE;

    /* One-wire bit slots are timing critical; defer ISRs for this transaction. */
    __disable_interrupt();
    if (reset_18B20())
    {
        send_18B20(0xCCU);   // Skip ROM command
        send_18B20(0x44U);   // Convert T command
        status = DS18B20_STATUS_OK;
    }

    if (interruptState)
        __enable_interrupt();

    return status;
}

Ds18b20Status ds18b20_read_temperature(float *temperatureC)
{
    unsigned int interruptState = __get_SR_register() & GIE;
    uint8_t scratchpad[DS18B20_SCRATCHPAD_BYTES];
    uint8_t crc = 0U;
    uint8_t i;
    uint16_t rawTemperature;
    uint16_t shiftedTemperature;
    int16_t signedTemperature;

    /* Protect reset and commands, then permit ISRs between scratchpad bytes. */
    __disable_interrupt();
    if (!reset_18B20())
    {
        if (interruptState)
            __enable_interrupt();
        return DS18B20_STATUS_NO_PRESENCE;
    }
    send_18B20(0xCCU);   // Skip ROM command
    send_18B20(0xBEU);   // Read Scratchpad command

    if (interruptState)
        __enable_interrupt();

    for (i = 0U; i < DS18B20_SCRATCHPAD_BYTES; i++)
    {
        __disable_interrupt();
        scratchpad[i] = read_18B20_byte();
        if (interruptState)
            __enable_interrupt();
    }

    for (i = 0U; i < DS18B20_CRC_DATA_BYTES; i++)
        crc = crc8_update(crc, scratchpad[i]);

    if (crc != scratchpad[DS18B20_CRC_DATA_BYTES])
        return DS18B20_STATUS_CRC_MISMATCH;

    rawTemperature = (uint16_t)scratchpad[0] |
                     ((uint16_t)scratchpad[1] << 8);

    /*
     * Preserve the existing effective 11-bit path: discard the raw LSB,
     * restore the sign bit, and convert the remaining 1/8-degree value.
     */
    shiftedTemperature = rawTemperature >> 1;
    if (shiftedTemperature & 0x4000U)
        shiftedTemperature |= 0x8000U;
    signedTemperature = (int16_t)shiftedTemperature;

    *temperatureC = (float)signedTemperature / 8.0f;
    return DS18B20_STATUS_OK;
}

Ds18b20Status get_temp(float *temperatureC)
{
    Ds18b20Status status = ds18b20_start_conversion();

    if (status != DS18B20_STATUS_OK)
        return status;

    // Blocking compatibility path. Temperature mode uses LPM3 instead.
    delay_us(750000UL);

    return ds18b20_read_temperature(temperatureC);
}

unsigned char reset_18B20(void)
{
    unsigned char present;

    ONE_WIRE_DIR |=ONE_WIRE_PIN;
    ONE_WIRE_OUT &= ~ONE_WIRE_PIN;
    __delay_cycles(DS18B20_RESET_LOW_CYCLES);
    ONE_WIRE_OUT |=ONE_WIRE_PIN;
    ONE_WIRE_DIR &= ~ONE_WIRE_PIN;
    __delay_cycles(DS18B20_PRESENCE_SAMPLE_CYCLES);
    present = (ONE_WIRE_IN & ONE_WIRE_PIN) == 0U;
    __delay_cycles(DS18B20_RESET_RECOVERY_CYCLES);

    return present;
}

void send_18B20(uint8_t data)
{
    uint8_t i;

    for(i = 0U; i < 8U; i++)
    {
        ONE_WIRE_DIR |=ONE_WIRE_PIN;
        ONE_WIRE_OUT &= ~ONE_WIRE_PIN;
        __delay_cycles(2);
        if(data & 0x01U)
        {
            ONE_WIRE_OUT |= ONE_WIRE_PIN;
        }
        __delay_cycles(60);
        ONE_WIRE_OUT |= ONE_WIRE_PIN;
        ONE_WIRE_DIR &= ~ONE_WIRE_PIN;
        data >>= 1;
    }
}

static uint8_t read_18B20_byte(void)
{
    uint8_t bit;
    uint8_t data = 0U;

    for (bit = 0U; bit < 8U; bit++)
    {
        ONE_WIRE_DIR |= ONE_WIRE_PIN;
        ONE_WIRE_OUT &= ~ONE_WIRE_PIN;
        __delay_cycles(DS18B20_READ_INIT_CYCLES);
        ONE_WIRE_OUT |=ONE_WIRE_PIN;
        ONE_WIRE_DIR &= ~ONE_WIRE_PIN;
        __delay_cycles(DS18B20_READ_SAMPLE_CYCLES);
        if(ONE_WIRE_IN & ONE_WIRE_PIN)
            data |= (uint8_t)(1U << bit);
        __delay_cycles(DS18B20_READ_REMAINDER_CYCLES);
    }

    return(data);
}

static uint8_t crc8_update(uint8_t crc, uint8_t byte)
{
    uint8_t bit;

    for (bit = 0U; bit < 8U; bit++)
    {
        uint8_t mix = (crc ^ byte) & 0x01U;

        crc >>= 1;
        if (mix)
            crc ^= 0x8CU;
        byte >>= 1;
    }

    return crc;
}
