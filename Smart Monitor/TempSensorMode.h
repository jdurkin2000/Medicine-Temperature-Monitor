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
 * TempSensorMode.h
 *
 * Simple thermometer application that uses a DS18B20 sensor to measure and
 * display temperature on the segmented LCD screen
 *
 * February 2015
 * E. Chen
 *
 ******************************************************************************/

#ifndef OUTOFBOX_MSP430FR6989_TEMPSENSORMODE_H_
#define OUTOFBOX_MSP430FR6989_TEMPSENSORMODE_H_

#include <stdint.h>

#define TEMPSENSOR_MODE 2
#define ACLK_FREQUENCY_HZ 32768UL

/*
 * Extra wait after each 750 ms DS18B20 conversion, in 32768 Hz ACLK ticks.
 * The short normal delay is convenient for prototyping; the alarm has no
 * extra delay so it samples as quickly as a 12-bit conversion allows.
 */
#define TEMP_NORMAL_UPDATE_TICKS 32768UL
#define TEMP_ALARM_UPDATE_TICKS 0UL

/*
 * Button timing is based on ACLK so holds continue to advance in LPM3.
 * These millisecond values are the user-adjustable input timings.
 */
#define BUTTON_TIMER_INTERVAL_MS 20U
#define BUTTON_LONG_PRESS_MS 1000U
#define BUTTON_RELEASE_DEBOUNCE_MS 40U
#define BUTTON_FEEDBACK_BEEP_MS 100U

#define BUTTON_LONG_PRESS_TICKS \
    ((BUTTON_LONG_PRESS_MS + BUTTON_TIMER_INTERVAL_MS - 1U) / \
     BUTTON_TIMER_INTERVAL_MS)
#define BUTTON_RELEASE_DEBOUNCE_TICKS \
    ((BUTTON_RELEASE_DEBOUNCE_MS + BUTTON_TIMER_INTERVAL_MS - 1U) / \
     BUTTON_TIMER_INTERVAL_MS)

/*
 * Alarm limits are stored in tenths of a degree Celsius. These defaults
 * implement the common 2 C to 8 C refrigerated-medicine storage range.
 */
extern volatile int16_t temp_alarm_low_tenths_c;
extern volatile int16_t temp_alarm_high_tenths_c;

extern volatile unsigned char mode;
extern volatile unsigned char S1buttonDebounce;
extern volatile unsigned char S2buttonDebounce;
extern volatile unsigned char tempUnit;

void tempSensor(void);
void tempSensorModeInit(void);
void tempSensorRequestDisplayRefresh(void);
unsigned char tempSensorIsAlarmActive(void);
void tempSensorAcknowledgeAlarm(void);
void tempSensorStartButtonFeedback(void);
unsigned char tempSensorAlarmDisplayIsActive(void);
unsigned char tempSensorAlarmDisplayNeedsTimer(void);
unsigned char tempSensorAlarmDisplayTimerTick(unsigned int elapsedMs);
unsigned char tempSensorAlarmDisplayServiceDisplay(void);
void displayTemp(void);

#endif /* OUTOFBOX_MSP430FR6989_TEMPSENSORMODE_H_ */
