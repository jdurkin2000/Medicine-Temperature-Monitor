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
#include "SettingsMode.h"
#include "TempSensorMode.h"
#include "hal_LCD.h"
#include "debug.h"
#include "ds18b20.h"

volatile unsigned char tempUnit = 0; // Temperature Unit
volatile int degC;                   // Celsius, in tenths of a degree
volatile int degF;                   // Fahrenheit, in tenths of a degree
volatile int16_t temp_alarm_low_tenths_c = 20;
volatile int16_t temp_alarm_high_tenths_c = 80;

#define DS18B20_CONVERSION_TICKS 24576UL // 750 ms at 32768 Hz
#define MEASUREMENT_LED_ON_TICKS 1638UL  // 50 ms green measurement flash
#define ALARM_ON_TICKS 3277UL            // 100 ms red LED/buzzer pulse
#define ALARM_OFF_TICKS 21299UL          // 650 ms between alarm pulses

/*
 * Connect a passive piezo buzzer between P1.3 (TA1.2, LaunchPad J4.1)
 * and GND. ACLK / 16 produces a 2.048 kHz tone that continues in LPM3.
 */
#define BUZZER_TIMER_PERIOD 15U
#define BUZZER_TIMER_DUTY 1U
#define LCD_EXCLAMATION_BIT BIT0
#define TEMPERATURE_HISTORY_MAGIC 0x544DU
#define TEMPERATURE_HISTORY_CAPACITY 64U
#define BUTTON_FEEDBACK_BEEP_ACLK_TICKS \
    ((ACLK_FREQUENCY_HZ * BUTTON_FEEDBACK_BEEP_MS + 999UL) / 1000UL)

typedef struct
{
    uint16_t magic;
    uint16_t nextIndex;
    uint16_t sampleCount;
    int16_t samplesTenthsC[TEMPERATURE_HISTORY_CAPACITY];
} TemperatureHistoryLog;

/*
 * PERSISTENT places this ring buffer in writable FRAM without C startup
 * reinitializing it after a reset. A new firmware load may still erase it.
 */
#if defined(__TI_COMPILER_VERSION__)
#pragma PERSISTENT(temperatureHistoryLog)
#endif
TemperatureHistoryLog temperatureHistoryLog = {0};

static volatile unsigned char lpm3DelayComplete;
static volatile unsigned char displayRefreshRequested;
static volatile unsigned char alarmAcknowledgeRequested;
static volatile unsigned char tempAlarmActive;
static volatile unsigned char feedbackToneRequested;
static unsigned char alarmArmed = 1;
static unsigned char alarmSignalOn;
static volatile unsigned char alarmToneRequested;
static volatile unsigned char buzzerToneOn;

/*
 * MSP430FR6989 erratum PMM32 requires LPM3/4 entry to execute from RAM after
 * putting FRAM into inactive mode. The linker command file already copies
 * .TI.ramfunc into RAM during startup.
 */
#if defined(__TI_COMPILER_VERSION__)
#pragma CODE_SECTION(enterLpm3FromRam, ".TI.ramfunc")
#endif
static void enterLpm3FromRam(unsigned short lowPowerMode)
{
    FRCTL0 = FRCTLPW;
    GCCTL0 &= ~(FRPWR | FRLPMPWR);
    FRCTL0_H = 0;
    __bis_SR_register(lowPowerMode);
}

static void updateBuzzerOutput(void)
{
    unsigned int interruptState = __get_SR_register() & GIE;
    unsigned char toneRequested;

    __disable_interrupt();
    toneRequested =
        alarmToneRequested || feedbackToneRequested;

    if (toneRequested)
    {
        if (!buzzerToneOn)
        {
            TA1CCR0 = BUZZER_TIMER_PERIOD;
            TA1CCR2 = BUZZER_TIMER_DUTY;
            TA1CCTL2 = OUTMOD_7;
            TA1CTL = TASSEL__ACLK | MC__UP | TACLR;
            buzzerToneOn = 1;
        }
    }
    else
    {
        TA1CTL = MC__STOP | TACLR;
        TA1CCTL2 = OUTMOD_0;
        P1OUT &= ~BIT3;
        buzzerToneOn = 0;
    }

    if (interruptState)
        __enable_interrupt();
}

static void setAlarmOutputs(unsigned char enabled)
{
    // The green measurement LED is never on during an alarm.
    P9OUT &= ~BIT7;

    if (enabled)
    {
        P1OUT |= BIT0;
    }
    else
    {
        P1OUT &= ~BIT0;
    }

    alarmToneRequested = enabled;
    updateBuzzerOutput();
}

static void initializeTemperatureHistory(void)
{
    if ((temperatureHistoryLog.magic != TEMPERATURE_HISTORY_MAGIC) ||
        (temperatureHistoryLog.nextIndex >= TEMPERATURE_HISTORY_CAPACITY) ||
        (temperatureHistoryLog.sampleCount > TEMPERATURE_HISTORY_CAPACITY))
    {
        temperatureHistoryLog.magic = 0;
        temperatureHistoryLog.nextIndex = 0;
        temperatureHistoryLog.sampleCount = 0;
        // Commit the validity marker last so an interrupted init retries.
        temperatureHistoryLog.magic = TEMPERATURE_HISTORY_MAGIC;
    }
}

static void logTemperatureSample(int16_t temperatureTenthsC)
{
    uint16_t writeIndex = temperatureHistoryLog.nextIndex;
    uint16_t followingIndex = writeIndex + 1U;

    if (followingIndex >= TEMPERATURE_HISTORY_CAPACITY)
        followingIndex = 0;

    temperatureHistoryLog.samplesTenthsC[writeIndex] = temperatureTenthsC;
    // nextIndex is always committed as a valid value, even at ring wrap.
    temperatureHistoryLog.nextIndex = followingIndex;

    if (temperatureHistoryLog.sampleCount < TEMPERATURE_HISTORY_CAPACITY)
        temperatureHistoryLog.sampleCount++;
}

static unsigned char serviceAlarmAcknowledgement(void)
{
    if (!alarmAcknowledgeRequested)
        return 0;

    alarmAcknowledgeRequested = 0;
    tempAlarmActive = 0;

    /*
     * Keep the alarm disarmed while the temperature remains out of range.
     * It rearms after a later measurement returns to the safe range.
     */
    alarmArmed = 0;
    alarmSignalOn = 0;
    setAlarmOutputs(0);
    settingsModeExit();
    displayTemp();
    return 1;
}

void tempSensorRequestDisplayRefresh(void)
{
    displayRefreshRequested = 1;
}

unsigned char tempSensorIsAlarmActive(void)
{
    return tempAlarmActive;
}

void tempSensorAcknowledgeAlarm(void)
{
    if (tempAlarmActive)
        alarmAcknowledgeRequested = 1;
}

void tempSensorStartButtonFeedback(void)
{
    unsigned int interruptState = __get_SR_register() & GIE;

    __disable_interrupt();

    // Timer B0 is a one-shot duration timer; Timer A1 generates the tone.
    TB0CTL = MC__STOP | TBCLR;
    TB0CCTL0 = 0;
    TB0CCR0 = (unsigned int)(BUTTON_FEEDBACK_BEEP_ACLK_TICKS - 1UL);
    feedbackToneRequested = 1;
    updateBuzzerOutput();
    TB0CCTL0 = CCIE;
    TB0CTL = TBSSEL__ACLK | MC__UP | TBCLR;

    if (interruptState)
        __enable_interrupt();
}

static void serviceDisplayRequests(void)
{
    unsigned int interruptState;
    unsigned char refreshTemperature;

    settingsModeServiceDisplay();

    interruptState = __get_SR_register() & GIE;
    __disable_interrupt();
    refreshTemperature = displayRefreshRequested;
    displayRefreshRequested = 0;
    if (interruptState)
        __enable_interrupt();

    if (refreshTemperature && !settingsModeIsActive())
        displayTemp();
}

static unsigned char sleepForAclkticks(unsigned long ticks)
{
    while (ticks != 0UL)
    {
        unsigned int interval;

        if (serviceAlarmAcknowledgement())
            return 0;

        serviceDisplayRequests();

        interval = (ticks > 0xFFFFUL) ? 0xFFFFU : (unsigned int)ticks;
        lpm3DelayComplete = 0;

        // Timer A3 is the one-shot sleep timer; Timer A1 drives the buzzer.
        TA3CTL = MC__STOP | TACLR;
        TA3CCTL0 = 0;
        TA3CCR0 = interval - 1U;
        TA3CCTL0 = CCIE;
        TA3CTL = TASSEL__ACLK | MC__UP | TACLR;

        /*
         * Disable interrupts while checking the flag to avoid an interrupt
         * occurring between the check and entry into LPM3. The instruction
         * that enters LPM3 also atomically re-enables interrupts.
         */
        __disable_interrupt();
        while (!lpm3DelayComplete)
        {
            if (alarmAcknowledgeRequested)
            {
                __enable_interrupt();
                serviceAlarmAcknowledgement();
                TA3CCTL0 = 0;
                TA3CTL = MC__STOP;
                return 0;
            }

            __enable_interrupt();
            serviceDisplayRequests();
            __disable_interrupt();

            if (lpm3DelayComplete)
                break;

            enterLpm3FromRam(LPM3_bits | GIE);
            __disable_interrupt();
        }
        __enable_interrupt();

        TA3CCTL0 = 0;
        TA3CTL = MC__STOP;
        serviceDisplayRequests();
        ticks -= interval;
    }

    return 1;
}

static unsigned char waitForAlarmConversion(void)
{
    unsigned long ticksRemaining = DS18B20_CONVERSION_TICKS;

    while (ticksRemaining != 0UL)
    {
        unsigned long phaseTicks = alarmSignalOn
                                       ? ALARM_ON_TICKS
                                       : ALARM_OFF_TICKS;
        unsigned long interval = (ticksRemaining > phaseTicks)
                                     ? phaseTicks
                                     : ticksRemaining;

        setAlarmOutputs(alarmSignalOn);
        if (!sleepForAclkticks(interval))
            return 0;

        alarmSignalOn ^= 1;
        ticksRemaining -= interval;
    }

    // End each conversion quietly and start the next one with a full pulse.
    setAlarmOutputs(0);
    alarmSignalOn = 1;
    return 1;
}

void tempSensor(void)
{
    while (mode == TEMPSENSOR_MODE)
    {
        float temperatureC;
        unsigned char temperatureInRange;

        serviceAlarmAcknowledgement();
        serviceDisplayRequests();

        if (tempAlarmActive)
        {
            P9OUT &= ~BIT7;
        }
        else
        {
            // Green LED marks the normal DS18B20 conversion period.
            setAlarmOutputs(0);
            P9OUT |= BIT7;
        }

        ds18b20_start_conversion();

        if (tempAlarmActive)
        {
            if (!waitForAlarmConversion())
                continue;
        }
        else
        {
            // Flash briefly, then finish the 750 ms conversion with LED off.
            sleepForAclkticks(MEASUREMENT_LED_ON_TICKS);
            P9OUT &= ~BIT7;
            sleepForAclkticks(DS18B20_CONVERSION_TICKS -
                              MEASUREMENT_LED_ON_TICKS);
        }

        if (mode != TEMPSENSOR_MODE)
            break;

        if (serviceAlarmAcknowledgement())
            continue;

        temperatureC = ds18b20_read_temperature();

        // Store both values in tenths of a degree for displayTemp().
        if (temperatureC >= 0.0f)
            degC = (int)(temperatureC * 10.0f + 0.5f);
        else
            degC = (int)(temperatureC * 10.0f - 0.5f);

        degF = degC * 9 / 5 + 320;

        // Log every completed sample, even while the settings LCD is active.
        logTemperatureSample((int16_t)degC);

        uartSendFloat(degF / 10.0, 2);
        uartSendChar('F');

        temperatureInRange =
            (degC >= temp_alarm_low_tenths_c) &&
            (degC <= temp_alarm_high_tenths_c);

        if (temperatureInRange)
        {
            alarmArmed = 1;
        }
        else if (alarmArmed)
        {
            tempAlarmActive = 1;
            alarmSignalOn = 1;
            settingsModeExit();
            P9OUT &= ~BIT7;
            setAlarmOutputs(1);
        }

        serviceDisplayRequests();
        if (!settingsModeIsActive())
            displayTemp();

        if (!tempAlarmActive)
        {
            P9OUT &= ~BIT7;
            // Short prototype interval; increase this for deployment.
            sleepForAclkticks(TEMP_NORMAL_UPDATE_TICKS);
        }
        else
        {
            sleepForAclkticks(TEMP_ALARM_UPDATE_TICKS);
        }
    }

    setAlarmOutputs(0);
}

void tempSensorModeInit(void)
{
    RTC_C_holdClock(RTC_C_BASE); // Stop stopwatch
    RTC_C_holdCounterPrescale(RTC_C_BASE, RTC_C_PRESCALE_0);

    // P1.3/TA1.2 is the hardware PWM output for the passive buzzer.
    GPIO_setAsPeripheralModuleFunctionOutputPin(
        GPIO_PORT_P1,
        GPIO_PIN3,
        GPIO_PRIMARY_MODULE_FUNCTION);
    TA1CTL = MC__STOP | TACLR;
    TA1CCTL2 = OUTMOD_0;

    initializeTemperatureHistory();
    tempAlarmActive = 0;
    alarmAcknowledgeRequested = 0;
    alarmArmed = 1;
    alarmSignalOn = 0;
    alarmToneRequested = 0;
    feedbackToneRequested = 0;
    buzzerToneOn = 0;
    TB0CTL = MC__STOP | TBCLR;
    TB0CCTL0 = 0;
    updateBuzzerOutput();
    P1OUT &= ~BIT0;
    P9OUT &= ~BIT7;
}

void displayTemp(void)
{
    if (settingsModeIsActive())
        return;

    clearLCD();

    // Pick C or F depending on tempUnit state
    int deg;
    if (tempUnit == 0)
    {
        showChar('C', pos6);
        deg = degC;
    }
    else
    {
        showChar('F', pos6);
        deg = degF;
    }

    // Handle negative values
    if (deg < 0)
    {
        deg *= -1;
        // Negative sign
        LCDMEM[pos1 + 1] |= 0x04;
    }

    // Handles displaying up to 999.9 degrees
    if (deg >= 1000)
        showChar((deg / 1000) % 10 + '0', pos2);
    if (deg >= 100)
        showChar((deg / 100) % 10 + '0', pos3);
    if (deg >= 10)
        showChar((deg / 10) % 10 + '0', pos4);
    if (deg >= 1)
        showChar((deg / 1) % 10 + '0', pos5);

    // Decimal point
    LCDMEM[pos4 + 1] |= 0x01;

    // The degree symbol is a fixed glass segment between positions 5 and 6.
    LCDMEM[pos5 + 1] |= 0x04;

    if (tempAlarmActive)
        LCDM3 |= LCD_EXCLAMATION_BIT;
}

#pragma vector = TIMER3_A0_VECTOR
__interrupt void TIMER3_A0_ISR(void)
{
    TA3CTL = MC__STOP;
    TA3CCTL0 &= ~CCIE;
    lpm3DelayComplete = 1;
    __bic_SR_register_on_exit(LPM3_bits);
}

#pragma vector = TIMER0_B0_VECTOR
__interrupt void TIMER0_B0_ISR(void)
{
    TB0CTL = MC__STOP | TBCLR;
    TB0CCTL0 &= ~CCIE;
    feedbackToneRequested = 0;
    updateBuzzerOutput();
}
