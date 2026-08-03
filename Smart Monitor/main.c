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
 * main.c
 *
 * Out of Box Demo for the MSP-EXP430FR6989
 * Main loop, initialization, and interrupt service routines
 *
 * This demo provides 2 application modes: Stopwatch Mode and Temperature Mode
 *
 * The stopwatch mode provides a simple stopwatch application that supports split
 * time, where the display freezes while the stopwatch continues running in the
 * background.
 *
 * The temperature mode provides a simple thermometer application using the
 * on-chip temperature sensor. Display toggles between C/F.
 *
 * February 2015
 * E. Chen
 *
 ******************************************************************************/

#include <driverlib.h>
#include "SettingsMode.h"
#include "TempSensorMode.h"
#include "hal_LCD.h"

#define ACLK_TICKS_FROM_MS(milliseconds) \
    ((ACLK_FREQUENCY_HZ * (milliseconds) + 999UL) / 1000UL)
#define BUTTON_TIMER_CCR0 \
    ((unsigned int)(ACLK_TICKS_FROM_MS(BUTTON_TIMER_INTERVAL_MS) - 1UL))
#define SETTINGS_SCROLL_TIMER_CCR0 \
    ((unsigned int)(ACLK_TICKS_FROM_MS(SETTINGS_SCROLL_STEP_MS) - 1UL))

volatile unsigned char mode = TEMPSENSOR_MODE;
volatile unsigned char S1buttonDebounce = 0;
volatile unsigned char S2buttonDebounce = 0;
volatile unsigned int counter = 0;
volatile int centisecond = 0;
Calendar currentTime;

static volatile unsigned char buttonTimerRunning;
static volatile unsigned int buttonTimerIntervalMs;
static volatile unsigned int s1HoldTicks;
static volatile unsigned int s2HoldTicks;
static volatile unsigned char s1ReleaseTicks;
static volatile unsigned char s2ReleaseTicks;
static volatile unsigned char s1LongHandled;
static volatile unsigned char s2LongHandled;
static volatile unsigned char s1PressConsumed;
static volatile unsigned char s2PressConsumed;

// Initialization calls
void Init_GPIO(void);
void Init_Clock(void);
void Init_UART(void);

/*
 * Main routine
 */
int main(void) {
    // Stop watchdog timer
    WDT_A_hold(__MSP430_BASEADDRESS_WDT_A__);

    // Finish all peripheral and UI state initialization before enabling ISRs.
    Init_GPIO();
    Init_Clock();
    Init_UART();
    Init_LCD();
    settingsModeInit();
    tempSensorModeInit();

    GPIO_clearInterrupt(GPIO_PORT_P1, GPIO_PIN1);
    GPIO_clearInterrupt(GPIO_PORT_P1, GPIO_PIN2);

    __enable_interrupt();

    while(1)
    {
        LCD_C_selectDisplayMemory(LCD_C_BASE, LCD_C_DISPLAYSOURCE_MEMORY);
        switch(mode)
        {
            case TEMPSENSOR_MODE:        // Temperature Sensor mode
                clearLCD();              // Clear all LCD segments
                tempSensor();
                break;
        }
    }
}


void Init_UART(void) {
    // Configure UART pins
    P3SEL0 |= BIT4 | BIT5;
    P3SEL1 &= ~(BIT4 | BIT5);

    // Configure eUSCI_A1 for 9600 baud at 1MHz SMCLK
    UCA1CTLW0 = UCSWRST;                      // Put eUSCI in reset
    UCA1CTLW0 |= UCSSEL__SMCLK;               // SMCLK clock source
    UCA1BRW = 6;                              // 1MHz / 16 / 9600 = ~6.5
    UCA1MCTLW = 0x2000 | UCOS16 | UCBRF_8;    // Modulation settings
    UCA1CTLW0 &= ~UCSWRST;                    // Initialize eUSCI
}


/*
 * GPIO Initialization
 */
void Init_GPIO()
{
    // Set all GPIO pins to output low to prevent floating input and reduce power consumption
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P6, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P7, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P9, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);

    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P3, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P6, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P7, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P8, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);
    GPIO_setAsOutputPin(GPIO_PORT_P9, GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3|GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7);

    GPIO_setAsInputPin(GPIO_PORT_P3, GPIO_PIN5);

    // Configure button S1 (P1.1) interrupt
    GPIO_selectInterruptEdge(GPIO_PORT_P1, GPIO_PIN1, GPIO_HIGH_TO_LOW_TRANSITION);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN1);
    GPIO_clearInterrupt(GPIO_PORT_P1, GPIO_PIN1);
    GPIO_enableInterrupt(GPIO_PORT_P1, GPIO_PIN1);

    // Configure button S2 (P1.2) interrupt
    GPIO_selectInterruptEdge(GPIO_PORT_P1, GPIO_PIN2, GPIO_HIGH_TO_LOW_TRANSITION);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN2);
    GPIO_clearInterrupt(GPIO_PORT_P1, GPIO_PIN2);
    GPIO_enableInterrupt(GPIO_PORT_P1, GPIO_PIN2);

    // Set P4.1 and P4.2 as Secondary Module Function Input, LFXT.
    GPIO_setAsPeripheralModuleFunctionInputPin(
           GPIO_PORT_PJ,
           GPIO_PIN4 + GPIO_PIN5,
           GPIO_PRIMARY_MODULE_FUNCTION
           );

    // Disable the GPIO power-on default high-impedance mode
    // to activate previously configured port settings
    PMM_unlockLPM5();
}

/*
 * Clock System Initialization
 */
void Init_Clock()
{
    // Set DCO frequency to default 8MHz
    CS_setDCOFreq(CS_DCORSEL_0, CS_DCOFSEL_6);

    // Configure MCLK and SMCLK to 1MHz
    CS_initClockSignal(CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_8);
    CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_8);

    // Intializes the XT1 crystal oscillator
    CS_turnOnLFXT(CS_LFXT_DRIVE_3);
}

static void startButtonTimer(void)
{
    if (buttonTimerRunning &&
        (buttonTimerIntervalMs == BUTTON_TIMER_INTERVAL_MS))
        return;

    buttonTimerRunning = 1;
    buttonTimerIntervalMs = BUTTON_TIMER_INTERVAL_MS;
    TA0CTL = MC__STOP | TACLR;
    TA0CCR0 = BUTTON_TIMER_CCR0;
    TA0CCTL0 = CCIE;
    TA0CTL = TASSEL__ACLK | MC__UP | TACLR;
}

/*
 * TempSensorMode calls this when an alarm scroll begins in foreground code.
 * Leave an active 20 ms debounce timer untouched; the ISR will select the
 * 200 ms scroll period after both buttons finish debouncing.
 */
void timerA0RequestScrollService(void)
{
    unsigned int interruptState = __get_SR_register() & GIE;

    __disable_interrupt();
    if (!buttonTimerRunning)
    {
        buttonTimerRunning = 1;
        buttonTimerIntervalMs = SETTINGS_SCROLL_STEP_MS;
        TA0CTL = MC__STOP | TACLR;
        TA0CCR0 = SETTINGS_SCROLL_TIMER_CCR0;
        TA0CCTL0 = CCIE;
        TA0CTL = TASSEL__ACLK | MC__UP | TACLR;
    }

    if (interruptState)
        __enable_interrupt();
}

/*
 * RTC Interrupt Service Routine
 * Wakes up every ~10 milliseconds to update stowatch
 */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=RTC_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(RTC_VECTOR)))
#endif
void RTC_ISR(void)
{
    switch(__even_in_range(RTCIV, 16))
    {
    case RTCIV_NONE: break;      //No interrupts
    case RTCIV_RTCOFIFG: break;      //RTCOFIFG
    case RTCIV_RTCRDYIFG:             //RTCRDYIFG
        counter = RTCPS;
        centisecond = 0;
        __bic_SR_register_on_exit(LPM3_bits);
        break;
    case RTCIV_RTCTEVIFG:             //RTCEVIFG
        //Interrupts every minute
        __no_operation();
        break;
    case RTCIV_RTCAIFG:             //RTCAIFG
        __no_operation();
        break;
    case RTCIV_RT0PSIFG:
        centisecond = RTCPS - counter;
        __bic_SR_register_on_exit(LPM3_bits);
        break;     //RT0PSIFG
    case RTCIV_RT1PSIFG:
        __bic_SR_register_on_exit(LPM3_bits);
        break;     //RT1PSIFG

    default: break;
    }
}

/*
 * PORT1 Interrupt Service Routine
 * Handles S1 and S2 button press interrupts
 */
#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    switch(__even_in_range(P1IV, P1IV_P1IFG7))
    {
        case P1IV_NONE : break;
        case P1IV_P1IFG0 : break;
        case P1IV_P1IFG1 :    // Button S1 pressed
            if (((S1buttonDebounce) == 0) && !(P1IN & BIT1))
            {
                S1buttonDebounce = 1;
                s1HoldTicks = 0;
                s1ReleaseTicks = 0;
                s1LongHandled = 0;
                s1PressConsumed = 0;

                tempSensorStartButtonFeedback();

                if ((mode == TEMPSENSOR_MODE) &&
                    tempSensorIsAlarmActive())
                {
                    tempSensorAcknowledgeAlarm();
                    s1PressConsumed = 1;
                }

                startButtonTimer();
                __bic_SR_register_on_exit(LPM3_bits);
            }
            break;
        case P1IV_P1IFG2 :    // Button S2 pressed
            if (((S2buttonDebounce) == 0) && !(P1IN & BIT2))
            {
                S2buttonDebounce = 1;
                s2HoldTicks = 0;
                s2ReleaseTicks = 0;
                s2LongHandled = 0;
                s2PressConsumed = 0;

                tempSensorStartButtonFeedback();

                if ((mode == TEMPSENSOR_MODE) &&
                    tempSensorIsAlarmActive())
                {
                    tempSensorAcknowledgeAlarm();
                    s2PressConsumed = 1;
                }

                startButtonTimer();
                __bic_SR_register_on_exit(LPM3_bits);
            }
            break;
        case P1IV_P1IFG3 : break;
        case P1IV_P1IFG4 : break;
        case P1IV_P1IFG5 : break;
        case P1IV_P1IFG6 : break;
        case P1IV_P1IFG7 : break;
    }
}

/*
 * Timer A0 Interrupt Service Routine
 * Used as button debounce timer
 */
#pragma vector = TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR (void)
{
    unsigned char wakeForeground =
        settingsModeTimerTick(buttonTimerIntervalMs);

    wakeForeground |=
        tempSensorAlarmDisplayTimerTick(buttonTimerIntervalMs);

    if (S1buttonDebounce)
    {
        if (!(P1IN & BIT1))
        {
            s1ReleaseTicks = 0;

            if (!s1LongHandled && !s1PressConsumed)
            {
                if (s1HoldTicks < BUTTON_LONG_PRESS_TICKS)
                    s1HoldTicks++;

                if (s1HoldTicks >= BUTTON_LONG_PRESS_TICKS)
                {
                    s1LongHandled = 1;

                    if (settingsModeIsActive())
                    {
                        settingsModeExit();
                        tempSensorRequestDisplayRefresh();
                        wakeForeground = 1;
                    }
                    else if ((P1IN & BIT2) &&
                             !tempSensorIsAlarmActive())
                    {
                        settingsModeEnter();
                        wakeForeground = 1;
                    }
                }
            }
        }
        else
        {
            if (s1ReleaseTicks < BUTTON_RELEASE_DEBOUNCE_TICKS)
                s1ReleaseTicks++;

            if (s1ReleaseTicks >= BUTTON_RELEASE_DEBOUNCE_TICKS)
            {
                if (!s1LongHandled && !s1PressConsumed)
                {
                    if (tempSensorIsAlarmActive())
                    {
                        tempSensorAcknowledgeAlarm();
                        wakeForeground = 1;
                    }
                    else if (settingsModeIsActive())
                    {
                        settingsModeSelectOption();
                    }
                }

                P1IFG &= ~BIT1;
                S1buttonDebounce = 0;
                s1HoldTicks = 0;
                s1ReleaseTicks = 0;
            }
        }
    }

    if (S2buttonDebounce)
    {
        if (!(P1IN & BIT2))
        {
            s2ReleaseTicks = 0;

            if (!s2LongHandled && !s2PressConsumed)
            {
                if (s2HoldTicks < BUTTON_LONG_PRESS_TICKS)
                    s2HoldTicks++;

                if (s2HoldTicks >= BUTTON_LONG_PRESS_TICKS)
                {
                    s2LongHandled = 1;

                    if (settingsModeIsActive())
                    {
                        settingsModeExit();
                        tempSensorRequestDisplayRefresh();
                        wakeForeground = 1;
                    }
                }
            }
        }
        else
        {
            if (s2ReleaseTicks < BUTTON_RELEASE_DEBOUNCE_TICKS)
                s2ReleaseTicks++;

            if (s2ReleaseTicks >= BUTTON_RELEASE_DEBOUNCE_TICKS)
            {
                if (!s2LongHandled && !s2PressConsumed)
                {
                    if (tempSensorIsAlarmActive())
                    {
                        tempSensorAcknowledgeAlarm();
                        wakeForeground = 1;
                    }
                    else if (settingsModeIsActive())
                    {
                        settingsModeNextOption();
                        wakeForeground = 1;
                    }
                    else
                    {
                        tempUnit ^= 0x01;
                        tempSensorRequestDisplayRefresh();
                        wakeForeground = 1;
                    }
                }

                P1IFG &= ~BIT2;
                S2buttonDebounce = 0;
                s2HoldTicks = 0;
                s2ReleaseTicks = 0;
            }
        }
    }

    if (!S1buttonDebounce && !S2buttonDebounce)
    {
        if (settingsModeNeedsTimer() ||
            tempSensorAlarmDisplayNeedsTimer())
        {
            if (buttonTimerIntervalMs != SETTINGS_SCROLL_STEP_MS)
            {
                buttonTimerIntervalMs = SETTINGS_SCROLL_STEP_MS;
                TA0CTL = MC__STOP | TACLR;
                TA0CCR0 = SETTINGS_SCROLL_TIMER_CCR0;
                TA0CCTL0 = CCIE;
                TA0CTL = TASSEL__ACLK | MC__UP | TACLR;
            }
        }
        else
        {
            TA0CTL = MC__STOP | TACLR;
            TA0CCTL0 &= ~CCIE;
            buttonTimerRunning = 0;
            buttonTimerIntervalMs = 0;
        }
    }

    if ((mode == TEMPSENSOR_MODE) && wakeForeground)
        __bic_SR_register_on_exit(LPM3_bits);            // exit LPM3
}

/*
 * ADC 12 Interrupt Service Routine
 * Wake up from LPM3 to display temperature
 */
#pragma vector=ADC12_VECTOR
__interrupt void ADC12_ISR(void)
{
    switch(__even_in_range(ADC12IV,12))
    {
    case  0: break;                         // Vector  0:  No interrupt
    case  2: break;                         // Vector  2:  ADC12BMEMx Overflow
    case  4: break;                         // Vector  4:  Conversion time overflow
    case  6: break;                         // Vector  6:  ADC12BHI
    case  8: break;                         // Vector  8:  ADC12BLO
    case 10: break;                         // Vector 10:  ADC12BIN
    case 12:                                // Vector 12:  ADC12BMEM0 Interrupt
        ADC12_B_clearInterrupt(ADC12_B_BASE, 0, ADC12_B_IFG0);
        __bic_SR_register_on_exit(LPM3_bits);   // Exit active CPU
        break;                              // Clear CPUOFF bit from 0(SR)
    case 14: break;                         // Vector 14:  ADC12BMEM1
    case 16: break;                         // Vector 16:  ADC12BMEM2
    case 18: break;                         // Vector 18:  ADC12BMEM3
    case 20: break;                         // Vector 20:  ADC12BMEM4
    case 22: break;                         // Vector 22:  ADC12BMEM5
    case 24: break;                         // Vector 24:  ADC12BMEM6
    case 26: break;                         // Vector 26:  ADC12BMEM7
    case 28: break;                         // Vector 28:  ADC12BMEM8
    case 30: break;                         // Vector 30:  ADC12BMEM9
    case 32: break;                         // Vector 32:  ADC12BMEM10
    case 34: break;                         // Vector 34:  ADC12BMEM11
    case 36: break;                         // Vector 36:  ADC12BMEM12
    case 38: break;                         // Vector 38:  ADC12BMEM13
    case 40: break;                         // Vector 40:  ADC12BMEM14
    case 42: break;                         // Vector 42:  ADC12BMEM15
    case 44: break;                         // Vector 44:  ADC12BMEM16
    case 46: break;                         // Vector 46:  ADC12BMEM17
    case 48: break;                         // Vector 48:  ADC12BMEM18
    case 50: break;                         // Vector 50:  ADC12BMEM19
    case 52: break;                         // Vector 52:  ADC12BMEM20
    case 54: break;                         // Vector 54:  ADC12BMEM21
    case 56: break;                         // Vector 56:  ADC12BMEM22
    case 58: break;                         // Vector 58:  ADC12BMEM23
    case 60: break;                         // Vector 60:  ADC12BMEM24
    case 62: break;                         // Vector 62:  ADC12BMEM25
    case 64: break;                         // Vector 64:  ADC12BMEM26
    case 66: break;                         // Vector 66:  ADC12BMEM27
    case 68: break;                         // Vector 68:  ADC12BMEM28
    case 70: break;                         // Vector 70:  ADC12BMEM29
    case 72: break;                         // Vector 72:  ADC12BMEM30
    case 74: break;                         // Vector 74:  ADC12BMEM31
    case 76: break;                         // Vector 76:  ADC12BRDY
    default: break;
    }
}
