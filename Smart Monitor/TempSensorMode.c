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
#include "ds18b20.h"

/* main.c owns the shared Timer_A0 hardware and starts it on this request. */
void timerA0RequestScrollService(void);

volatile unsigned char tempUnit = 0; // Temperature Unit
volatile int degC;                   // Celsius, in tenths of a degree
volatile int degF;                   // Fahrenheit, in tenths of a degree
volatile int16_t temp_alarm_low_tenths_c = 210;
volatile int16_t temp_alarm_high_tenths_c = 260;

#define DS18B20_CONVERSION_TICKS 12288UL // 375 ms at 32768 Hz (11-bit maximum)
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
#define TEMPERATURE_HISTORY_MAGIC_LEGACY 0x544DU
#define TEMPERATURE_HISTORY_MAGIC 0x544EU
#define TEMPERATURE_HISTORY_CAPACITY 64U
#define TEMPERATURE_HISTORY_VALIDITY_BYTES \
    (TEMPERATURE_HISTORY_CAPACITY / 8U)
#define DS18B20_MAX_ATTEMPTS 3U
#define RELIABILITY_BUCKET_COUNT 60U
#define RELIABILITY_MINUTE_TICKS (ACLK_FREQUENCY_HZ * 60UL)
#define ALARM_CAUSE_TEMPERATURE BIT0
#define ALARM_CAUSE_SENSOR_NO_PRESENCE BIT1
#define ALARM_CAUSE_SENSOR_CRC_MISMATCH BIT2
#define ALARM_CAUSE_SIGNAL_DEGRADATION BIT3
#define LCD_CHARACTER_COUNT 6U
#define ALARM_SCROLL_FIRST_OFFSET 5
#define BUTTON_FEEDBACK_BEEP_ACLK_TICKS \
    ((ACLK_FREQUENCY_HZ * BUTTON_FEEDBACK_BEEP_MS + 999UL) / 1000UL)

typedef struct
{
    uint16_t magic;
    uint16_t nextIndex;
    uint16_t sampleCount;
    int16_t samplesTenthsC[TEMPERATURE_HISTORY_CAPACITY];
    uint8_t validityBitmap[TEMPERATURE_HISTORY_VALIDITY_BYTES];
} TemperatureHistoryLog;

typedef struct
{
    uint16_t successfulAttempts;
    uint16_t totalAttempts;
} ReliabilityBucket;

typedef enum
{
    ALARM_DISPLAY_MESSAGE_TEMP_HI = 0,
    ALARM_DISPLAY_MESSAGE_TEMP_LO,
    ALARM_DISPLAY_MESSAGE_NO_PROBE,
    ALARM_DISPLAY_MESSAGE_BAD_CRC,
    ALARM_DISPLAY_MESSAGE_WEAK_SIGNAL
} AlarmDisplayMessage;

static const int lcdPositions[LCD_CHARACTER_COUNT] =
{
    pos1, pos2, pos3, pos4, pos5, pos6
};

static const unsigned char alarmCauseOrder[] =
{
    ALARM_CAUSE_TEMPERATURE,
    ALARM_CAUSE_SENSOR_NO_PRESENCE,
    ALARM_CAUSE_SENSOR_CRC_MISMATCH,
    ALARM_CAUSE_SIGNAL_DEGRADATION
};

static const char alarmTempHighText[] = "TEMP HI";
static const char alarmTempLowText[] = "TEMP LO";
static const char alarmNoProbeText[] = "NO PROBE";
static const char alarmBadCrcText[] = "BAD CRC";
static const char alarmWeakSignalText[] = "WEAK SIGNAL";

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
static unsigned char alarmSignalOn;
static volatile unsigned char alarmToneRequested;
static volatile unsigned char buzzerToneOn;
static volatile unsigned char currentAlarmConditions;
static unsigned char acknowledgedAlarmConditions;
static unsigned char temperatureRangeCondition;
static unsigned char sensorNoPresenceCondition;
static unsigned char sensorCrcMismatchCondition;
static unsigned char signalDegradationCondition;
static unsigned char currentReadingInvalid;
static unsigned char hasLastGoodTemperature;
static int16_t lastGoodTemperatureTenthsC;
static volatile unsigned char alarmDisplayActive;
static volatile unsigned char alarmDisplayUpdateRequested;
static volatile unsigned int alarmDisplayElapsedMs;
static volatile signed char alarmDisplayOffset;
static volatile unsigned char alarmDisplayCycleCauses;
static volatile unsigned char alarmDisplayCause;
static volatile unsigned char alarmDisplayMessage;
static volatile unsigned char alarmDisplayMessageLength;
static ReliabilityBucket reliabilityBuckets[RELIABILITY_BUCKET_COUNT];
static uint8_t reliabilityCurrentBucket;
static uint8_t reliabilityElapsedMinutes;
static uint32_t reliabilityTicksInMinute;
static uint32_t reliabilitySuccessfulAttempts;
static uint32_t reliabilityTotalAttempts;

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

static unsigned char nextAlarmDisplayCause(unsigned char causes,
                                           unsigned char afterCause)
{
    unsigned char index = 0U;
    const unsigned char causeCount =
        (unsigned char)(sizeof(alarmCauseOrder) / sizeof(alarmCauseOrder[0]));

    if (afterCause != 0U)
    {
        while ((index < causeCount) &&
               (alarmCauseOrder[index] != afterCause))
        {
            index++;
        }

        if (index < causeCount)
            index++;
    }

    while (index < causeCount)
    {
        if ((causes & alarmCauseOrder[index]) != 0U)
            return alarmCauseOrder[index];
        index++;
    }

    return 0U;
}

static void prepareAlarmDisplayMessage(unsigned char cause)
{
    switch (cause)
    {
        case ALARM_CAUSE_TEMPERATURE:
            if (degC < temp_alarm_low_tenths_c)
            {
                alarmDisplayMessage = ALARM_DISPLAY_MESSAGE_TEMP_LO;
                alarmDisplayMessageLength =
                    (unsigned char)(sizeof(alarmTempLowText) - 1U);
            }
            else
            {
                alarmDisplayMessage = ALARM_DISPLAY_MESSAGE_TEMP_HI;
                alarmDisplayMessageLength =
                    (unsigned char)(sizeof(alarmTempHighText) - 1U);
            }
            break;
        case ALARM_CAUSE_SENSOR_NO_PRESENCE:
            alarmDisplayMessage = ALARM_DISPLAY_MESSAGE_NO_PROBE;
            alarmDisplayMessageLength =
                (unsigned char)(sizeof(alarmNoProbeText) - 1U);
            break;
        case ALARM_CAUSE_SENSOR_CRC_MISMATCH:
            alarmDisplayMessage = ALARM_DISPLAY_MESSAGE_BAD_CRC;
            alarmDisplayMessageLength =
                (unsigned char)(sizeof(alarmBadCrcText) - 1U);
            break;
        case ALARM_CAUSE_SIGNAL_DEGRADATION:
        default:
            alarmDisplayMessage = ALARM_DISPLAY_MESSAGE_WEAK_SIGNAL;
            alarmDisplayMessageLength =
                (unsigned char)(sizeof(alarmWeakSignalText) - 1U);
            break;
    }
}

static void renderAlarmScrollWindow(const char *text,
                                    unsigned char textLength,
                                    signed char offset)
{
    char characters[LCD_CHARACTER_COUNT] = {' ', ' ', ' ', ' ', ' ', ' '};
    unsigned char index = textLength;

    while (index != 0U)
    {
        int lcdIndex;

        index--;
        lcdIndex = (int)offset + (int)index;
        if ((lcdIndex >= 0) && (lcdIndex < (int)LCD_CHARACTER_COUNT))
            characters[(unsigned int)lcdIndex] = text[index];
    }

    clearLCD();
    index = LCD_CHARACTER_COUNT;
    while (index != 0U)
    {
        index--;
        showChar(characters[index], lcdPositions[index]);
    }
    LCDM3 |= LCD_EXCLAMATION_BIT;
}

static void startAlarmDisplay(void)
{
    unsigned int interruptState = __get_SR_register() & GIE;

    __disable_interrupt();
    alarmDisplayCycleCauses = currentAlarmConditions;
    alarmDisplayCause = nextAlarmDisplayCause(alarmDisplayCycleCauses, 0U);
    prepareAlarmDisplayMessage(alarmDisplayCause);
    alarmDisplayOffset = ALARM_SCROLL_FIRST_OFFSET;
    alarmDisplayElapsedMs = 0U;
    alarmDisplayUpdateRequested = 1U;
    alarmDisplayActive = 1U;
    if (interruptState)
        __enable_interrupt();

    timerA0RequestScrollService();
}

static void stopAlarmDisplay(void)
{
    unsigned int interruptState = __get_SR_register() & GIE;

    __disable_interrupt();
    alarmDisplayActive = 0U;
    alarmDisplayUpdateRequested = 0U;
    alarmDisplayElapsedMs = 0U;
    if (interruptState)
        __enable_interrupt();
}

unsigned char tempSensorAlarmDisplayIsActive(void)
{
    return alarmDisplayActive;
}

unsigned char tempSensorAlarmDisplayNeedsTimer(void)
{
    return alarmDisplayActive;
}

unsigned char tempSensorAlarmDisplayTimerTick(unsigned int elapsedMs)
{
    signed char lastOffset;
    unsigned char nextCause;
    unsigned char refreshedCauses;

    if (!alarmDisplayActive)
        return 0U;

    alarmDisplayElapsedMs += elapsedMs;
    if (alarmDisplayElapsedMs < SETTINGS_SCROLL_STEP_MS)
        return 0U;

    alarmDisplayElapsedMs -= SETTINGS_SCROLL_STEP_MS;
    lastOffset = (signed char)(-((signed char)alarmDisplayMessageLength - 1));

    if (alarmDisplayOffset > lastOffset)
    {
        alarmDisplayOffset--;
    }
    else
    {
        nextCause = nextAlarmDisplayCause(alarmDisplayCycleCauses,
                                         alarmDisplayCause);
        if (nextCause == 0U)
        {
            refreshedCauses = currentAlarmConditions;
            if (refreshedCauses != 0U)
                alarmDisplayCycleCauses = refreshedCauses;

            nextCause = nextAlarmDisplayCause(alarmDisplayCycleCauses, 0U);
        }

        alarmDisplayCause = nextCause;
        prepareAlarmDisplayMessage(alarmDisplayCause);
        alarmDisplayOffset = ALARM_SCROLL_FIRST_OFFSET;
    }

    alarmDisplayUpdateRequested = 1U;
    return 1U;
}

unsigned char tempSensorAlarmDisplayServiceDisplay(void)
{
    unsigned int interruptState;
    unsigned char active;
    unsigned char message;
    unsigned char messageLength;
    signed char offset;
    const char *text;

    /* Claim one pending frame atomically; LCD rendering stays in foreground. */
    interruptState = __get_SR_register() & GIE;
    __disable_interrupt();
    if (!alarmDisplayUpdateRequested)
    {
        if (interruptState)
            __enable_interrupt();
        return 0U;
    }

    alarmDisplayUpdateRequested = 0U;
    active = alarmDisplayActive;
    message = alarmDisplayMessage;
    messageLength = alarmDisplayMessageLength;
    offset = alarmDisplayOffset;

    if (!active)
    {
        if (interruptState)
            __enable_interrupt();
        return 0U;
    }

    switch ((AlarmDisplayMessage)message)
    {
        case ALARM_DISPLAY_MESSAGE_TEMP_HI:
            text = alarmTempHighText;
            break;
        case ALARM_DISPLAY_MESSAGE_TEMP_LO:
            text = alarmTempLowText;
            break;
        case ALARM_DISPLAY_MESSAGE_NO_PROBE:
            text = alarmNoProbeText;
            break;
        case ALARM_DISPLAY_MESSAGE_BAD_CRC:
            text = alarmBadCrcText;
            break;
        case ALARM_DISPLAY_MESSAGE_WEAK_SIGNAL:
        default:
            text = alarmWeakSignalText;
            break;
    }

    renderAlarmScrollWindow(text, messageLength, offset);

    if (interruptState)
        __enable_interrupt();
    return 1U;
}

static void activateAlarm(void)
{
    if (tempAlarmActive)
        return;

    tempAlarmActive = 1;
    alarmSignalOn = 1;
    settingsModeExit();
    P9OUT &= ~BIT7;
    setAlarmOutputs(1);
    startAlarmDisplay();
}

static void refreshAlarmConditions(void)
{
    unsigned char updatedConditions = 0U;
    unsigned char unacknowledgedConditions;

    if (temperatureRangeCondition)
        updatedConditions |= ALARM_CAUSE_TEMPERATURE;
    if (sensorNoPresenceCondition)
        updatedConditions |= ALARM_CAUSE_SENSOR_NO_PRESENCE;
    if (sensorCrcMismatchCondition)
        updatedConditions |= ALARM_CAUSE_SENSOR_CRC_MISMATCH;
    if (signalDegradationCondition)
        updatedConditions |= ALARM_CAUSE_SIGNAL_DEGRADATION;

    /* Timer_A0 may snapshot this byte, so publish the complete mask at once. */
    currentAlarmConditions = updatedConditions;

    /* A cleared condition rearms that cause for a future assertion. */
    acknowledgedAlarmConditions &= updatedConditions;
    unacknowledgedConditions =
        updatedConditions & (unsigned char)~acknowledgedAlarmConditions;

    if (unacknowledgedConditions != 0U)
        activateAlarm();
}

static unsigned char historySampleIsValid(uint16_t index)
{
    uint8_t byteIndex = (uint8_t)(index >> 3);
    uint8_t bitMask = (uint8_t)(1U << (index & 0x07U));

    return (temperatureHistoryLog.validityBitmap[byteIndex] & bitMask) != 0U;
}

static void setHistorySampleValidity(uint16_t index, unsigned char valid)
{
    uint8_t byteIndex = (uint8_t)(index >> 3);
    uint8_t bitMask = (uint8_t)(1U << (index & 0x07U));

    if (valid)
        temperatureHistoryLog.validityBitmap[byteIndex] |= bitMask;
    else
        temperatureHistoryLog.validityBitmap[byteIndex] &=
            (uint8_t)~bitMask;
}

static void clearHistoryValidity(void)
{
    uint8_t i;

    for (i = 0U; i < TEMPERATURE_HISTORY_VALIDITY_BYTES; i++)
        temperatureHistoryLog.validityBitmap[i] = 0U;
}

static void markRetainedLegacySamplesValid(void)
{
    uint16_t index = temperatureHistoryLog.nextIndex;
    uint16_t remaining = temperatureHistoryLog.sampleCount;

    clearHistoryValidity();
    while (remaining != 0U)
    {
        index = (index == 0U) ?
                    (TEMPERATURE_HISTORY_CAPACITY - 1U) :
                    (index - 1U);
        setHistorySampleValidity(index, 1U);
        remaining--;
    }
}

static void initializeTemperatureHistory(void)
{
    unsigned char structureValid =
        (temperatureHistoryLog.nextIndex < TEMPERATURE_HISTORY_CAPACITY) &&
        (temperatureHistoryLog.sampleCount <= TEMPERATURE_HISTORY_CAPACITY);

    if ((temperatureHistoryLog.magic == TEMPERATURE_HISTORY_MAGIC_LEGACY) &&
        structureValid)
    {
        /* Existing entries predate validity metadata, so migrate as valid. */
        temperatureHistoryLog.magic = 0U;
        markRetainedLegacySamplesValid();
        temperatureHistoryLog.magic = TEMPERATURE_HISTORY_MAGIC;
    }
    else if ((temperatureHistoryLog.magic != TEMPERATURE_HISTORY_MAGIC) ||
             !structureValid)
    {
        temperatureHistoryLog.magic = 0U;
        temperatureHistoryLog.nextIndex = 0U;
        temperatureHistoryLog.sampleCount = 0U;
        clearHistoryValidity();
        // Commit the validity marker last so an interrupted init retries.
        temperatureHistoryLog.magic = TEMPERATURE_HISTORY_MAGIC;
    }
}

static void recoverLastGoodTemperature(void)
{
    uint16_t index = temperatureHistoryLog.nextIndex;
    uint16_t remaining = temperatureHistoryLog.sampleCount;

    hasLastGoodTemperature = 0U;
    while (remaining != 0U)
    {
        index = (index == 0U) ?
                    (TEMPERATURE_HISTORY_CAPACITY - 1U) :
                    (index - 1U);
        if (historySampleIsValid(index))
        {
            lastGoodTemperatureTenthsC =
                temperatureHistoryLog.samplesTenthsC[index];
            degC = lastGoodTemperatureTenthsC;
            degF = degC * 9 / 5 + 320;
            hasLastGoodTemperature = 1U;
            return;
        }
        remaining--;
    }
}

static void logTemperatureSample(int16_t temperatureTenthsC,
                                 unsigned char valid)
{
    uint16_t writeIndex = temperatureHistoryLog.nextIndex;
    uint16_t followingIndex = writeIndex + 1U;

    if (followingIndex >= TEMPERATURE_HISTORY_CAPACITY)
        followingIndex = 0;

    temperatureHistoryLog.samplesTenthsC[writeIndex] = temperatureTenthsC;
    setHistorySampleValidity(writeIndex, valid);
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
    stopAlarmDisplay();
    tempAlarmActive = 0;

    /*
     * Acknowledge every cause that is currently asserted. Each cause rearms
     * independently after its condition clears.
     */
    acknowledgedAlarmConditions |= currentAlarmConditions;
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

static void evaluateReadingReliability(void)
{
    unsigned char unreliable = 0U;

    if ((reliabilityElapsedMinutes >= RELIABILITY_BUCKET_COUNT) &&
        (reliabilityTotalAttempts != 0UL))
    {
        unreliable =
            (reliabilitySuccessfulAttempts * 10UL) <
            (reliabilityTotalAttempts * 9UL);
    }

    signalDegradationCondition = unreliable;
    refreshAlarmConditions();
}

static void initializeReadingReliability(void)
{
    uint8_t i;

    for (i = 0U; i < RELIABILITY_BUCKET_COUNT; i++)
    {
        reliabilityBuckets[i].successfulAttempts = 0U;
        reliabilityBuckets[i].totalAttempts = 0U;
    }

    reliabilityCurrentBucket = 0U;
    reliabilityElapsedMinutes = 0U;
    reliabilityTicksInMinute = 0UL;
    reliabilitySuccessfulAttempts = 0UL;
    reliabilityTotalAttempts = 0UL;
    signalDegradationCondition = 0U;
}

static void recordReadAttempt(unsigned char successful)
{
    ReliabilityBucket *bucket =
        &reliabilityBuckets[reliabilityCurrentBucket];

    bucket->totalAttempts++;
    reliabilityTotalAttempts++;
    if (successful)
    {
        bucket->successfulAttempts++;
        reliabilitySuccessfulAttempts++;
    }

    evaluateReadingReliability();
}

static void advanceReadingReliability(unsigned long elapsedTicks)
{
    reliabilityTicksInMinute += elapsedTicks;

    while (reliabilityTicksInMinute >= RELIABILITY_MINUTE_TICKS)
    {
        ReliabilityBucket *expiredBucket;

        reliabilityTicksInMinute -= RELIABILITY_MINUTE_TICKS;
        if (reliabilityElapsedMinutes < RELIABILITY_BUCKET_COUNT)
            reliabilityElapsedMinutes++;

        reliabilityCurrentBucket++;
        if (reliabilityCurrentBucket >= RELIABILITY_BUCKET_COUNT)
            reliabilityCurrentBucket = 0U;

        expiredBucket = &reliabilityBuckets[reliabilityCurrentBucket];
        reliabilitySuccessfulAttempts -= expiredBucket->successfulAttempts;
        reliabilityTotalAttempts -= expiredBucket->totalAttempts;
        expiredBucket->successfulAttempts = 0U;
        expiredBucket->totalAttempts = 0U;
    }

    evaluateReadingReliability();
}

static void serviceDisplayRequests(void)
{
    unsigned int interruptState;
    unsigned char refreshTemperature;

    settingsModeServiceDisplay();
    tempSensorAlarmDisplayServiceDisplay();

    interruptState = __get_SR_register() & GIE;
    __disable_interrupt();
    refreshTemperature = displayRefreshRequested;
    displayRefreshRequested = 0;
    if (interruptState)
        __enable_interrupt();

    if (refreshTemperature && !settingsModeIsActive() &&
        !tempSensorAlarmDisplayIsActive())
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
        advanceReadingReliability(interval);
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
        Ds18b20Status sensorStatus = DS18B20_STATUS_NO_PRESENCE;
        uint8_t attempt;
        unsigned char conversionStarted = 0U;
        unsigned char measurementValid = 0U;

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

        for (attempt = 0U; attempt < DS18B20_MAX_ATTEMPTS; attempt++)
        {
            sensorStatus = ds18b20_start_conversion();
            if (sensorStatus == DS18B20_STATUS_OK)
            {
                conversionStarted = 1U;
                recordReadAttempt(1U);
                break;
            }

            recordReadAttempt(0U);
        }

        if (tempAlarmActive)
        {
            if (!waitForAlarmConversion())
            {
                ds18b20_power_off();
                continue;
            }
        }
        else
        {
            // Flash briefly, then finish the 375 ms conversion with LED off.
            if (!sleepForAclkticks(MEASUREMENT_LED_ON_TICKS))
            {
                ds18b20_power_off();
                continue;
            }
            P9OUT &= ~BIT7;
            if (!sleepForAclkticks(DS18B20_CONVERSION_TICKS -
                                   MEASUREMENT_LED_ON_TICKS))
            {
                ds18b20_power_off();
                continue;
            }
        }

        if (mode != TEMPSENSOR_MODE)
        {
            ds18b20_power_off();
            break;
        }

        if (serviceAlarmAcknowledgement())
        {
            ds18b20_power_off();
            continue;
        }

        if (conversionStarted)
        {
            for (attempt = 0U; attempt < DS18B20_MAX_ATTEMPTS; attempt++)
            {
                sensorStatus = ds18b20_read_temperature(&temperatureC);
                measurementValid = sensorStatus == DS18B20_STATUS_OK;
                recordReadAttempt(measurementValid);
                if (measurementValid)
                    break;
            }
        }
        ds18b20_power_off();

        if (measurementValid)
        {
            // Store both values in tenths of a degree for displayTemp().
            if (temperatureC >= 0.0f)
                degC = (int)(temperatureC * 10.0f + 0.5f);
            else
                degC = (int)(temperatureC * 10.0f - 0.5f);

            degF = degC * 9 / 5 + 320;
            lastGoodTemperatureTenthsC = (int16_t)degC;
            hasLastGoodTemperature = 1U;
            currentReadingInvalid = 0U;
            sensorNoPresenceCondition = 0U;
            sensorCrcMismatchCondition = 0U;

            // Log one validated sample even while settings owns the LCD.
            logTemperatureSample((int16_t)degC, 1U);

            // uartSendFloat(degF / 10.0, 2);
            // uartSendChar('F');

            temperatureRangeCondition =
                !((degC >= temp_alarm_low_tenths_c) &&
                  (degC <= temp_alarm_high_tenths_c));
            refreshAlarmConditions();
        }
        else
        {
            int16_t retainedTemperature = hasLastGoodTemperature ?
                                              lastGoodTemperatureTenthsC : 0;

            currentReadingInvalid = 1U;
            settingsModeExit();
            logTemperatureSample(retainedTemperature, 0U);
            sensorNoPresenceCondition =
                sensorStatus == DS18B20_STATUS_NO_PRESENCE;
            sensorCrcMismatchCondition =
                sensorStatus == DS18B20_STATUS_CRC_MISMATCH;
            refreshAlarmConditions();
        }

        serviceDisplayRequests();
        if (!settingsModeIsActive() &&
            !tempSensorAlarmDisplayIsActive())
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

    ds18b20_power_off();
    setAlarmOutputs(0);
}

void tempSensorModeInit(void)
{
    RTC_C_holdClock(RTC_C_BASE); // Stop stopwatch
    RTC_C_holdCounterPrescale(RTC_C_BASE, RTC_C_PRESCALE_0);
    ds18b20_init();

    // P1.3/TA1.2 is the hardware PWM output for the passive buzzer.
    GPIO_setAsPeripheralModuleFunctionOutputPin(
        GPIO_PORT_P1,
        GPIO_PIN3,
        GPIO_PRIMARY_MODULE_FUNCTION);
    TA1CTL = MC__STOP | TACLR;
    TA1CCTL2 = OUTMOD_0;

    initializeTemperatureHistory();
    recoverLastGoodTemperature();
    tempAlarmActive = 0U;
    alarmAcknowledgeRequested = 0U;
    alarmSignalOn = 0U;
    alarmToneRequested = 0U;
    feedbackToneRequested = 0U;
    buzzerToneOn = 0U;
    currentAlarmConditions = 0U;
    acknowledgedAlarmConditions = 0U;
    temperatureRangeCondition = 0U;
    sensorNoPresenceCondition = 0U;
    sensorCrcMismatchCondition = 0U;
    signalDegradationCondition = 0U;
    currentReadingInvalid = 1U;
    displayRefreshRequested = hasLastGoodTemperature ? 1U : 0U;
    alarmDisplayActive = 0U;
    alarmDisplayUpdateRequested = 0U;
    alarmDisplayElapsedMs = 0U;
    alarmDisplayOffset = ALARM_SCROLL_FIRST_OFFSET;
    alarmDisplayCycleCauses = 0U;
    alarmDisplayCause = 0U;
    alarmDisplayMessage = ALARM_DISPLAY_MESSAGE_TEMP_HI;
    alarmDisplayMessageLength =
        (unsigned char)(sizeof(alarmTempHighText) - 1U);
    initializeReadingReliability();
    TB0CTL = MC__STOP | TBCLR;
    TB0CCTL0 = 0;
    updateBuzzerOutput();
    P1OUT &= ~BIT0;
    P9OUT &= ~BIT7;
}

void displayTemp(void)
{
    if (settingsModeIsActive() || tempSensorAlarmDisplayIsActive())
        return;

    clearLCD();

    if (currentReadingInvalid && !hasLastGoodTemperature)
    {
        showChar('F', pos1);
        showChar('A', pos2);
        showChar('U', pos3);
        showChar('L', pos4);
        showChar('T', pos5);
        LCDM3 |= LCD_EXCLAMATION_BIT;
        return;
    }

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

    if (currentReadingInvalid || tempAlarmActive)
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
