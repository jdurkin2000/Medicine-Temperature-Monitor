/* --COPYRIGHT--,BSD
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/*******************************************************************************
 *
 * TempSensorMode.c
 *
 * Simple thermometer application that uses a DS18B20 sensor to measure and
 * display temperature on the segmented LCD screen
 *
 * February 2015
 * E. Chen
 *
 ******************************************************************************/

#include <driverlib.h>
#include "TempSensorMode.h"
#include "hal_LCD.h"

#include "ds18b20.h"

volatile unsigned char tempUnit = 0;         // Temperature Unit
volatile int degC;                           // Celsius, in tenths of a degree
volatile int degF;                           // Fahrenheit, in tenths of a degree

#define TEMPERATURE_UPDATE_INTERVAL_US 1000000UL

void tempSensor(void)
{
    while(mode == TEMPSENSOR_MODE)
    {
        float temperatureC;

        // Turn LED1 on while reading and updating the temperature.
        P1OUT |= BIT0;

        temperatureC = get_temp();

        // Store both values in tenths of a degree for displayTemp().
        if (temperatureC >= 0.0f)
            degC = (int)(temperatureC * 10.0f + 0.5f);
        else
            degC = (int)(temperatureC * 10.0f - 0.5f);

        degF = degC * 9 / 5 + 320;

        displayTemp();
        P1OUT &= ~BIT0;

        // Keep the display responsive without continuously polling the sensor.
        delay_us(TEMPERATURE_UPDATE_INTERVAL_US);
    }
}

void tempSensorModeInit(void)
{
    displayScrollText("TEMPSENSOR MODE");

    RTC_C_holdClock(RTC_C_BASE);                           // Stop stopwatch
    RTC_C_holdCounterPrescale(RTC_C_BASE, RTC_C_PRESCALE_0);

    // Check if any button is pressed
    Timer_A_initUpMode(TIMER_A0_BASE, &initUpParam_A0);
}

void displayTemp(void)
{
    clearLCD();

    // Pick C or F depending on tempUnit state
    int deg;
    if (tempUnit == 0)
    {
        showChar('C',pos6);
        deg = degC;
    }
    else
    {
        showChar('F',pos6);
        deg = degF;
    }

    // Handle negative values
    if (deg < 0)
    {
        deg *= -1;
        // Negative sign
        LCDMEM[pos1+1] |= 0x04;
    }

    // Handles displaying up to 999.9 degrees
    if (deg>=1000)
        showChar((deg/1000)%10 + '0',pos2);
    if (deg>=100)
        showChar((deg/100)%10 + '0',pos3);
    if (deg>=10)
        showChar((deg/10)%10 + '0',pos4);
    if (deg>=1)
        showChar((deg/1)%10 + '0',pos5);

    // Decimal point
    LCDMEM[pos4+1] |= 0x01;

    // Degree symbol
    LCDMEM[pos5+1] |= 0x04;
}
