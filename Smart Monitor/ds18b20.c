// Taken from https://www.embeddedrelated.com/showcode/294.php

#include <driverlib.h>
#include "ds18b20.h"

float get_temp(void);
void ds18b20_start_conversion(void);
float ds18b20_read_temperature(void);
void delay_us(unsigned long microseconds);
void reset_18B20(void);
void send_18B20(char data);
unsigned int read_18B20(void);

#define ONE_WIRE_PIN BIT4
#define ONE_WIRE_IN P1IN
#define ONE_WIRE_OUT P1OUT
#define ONE_WIRE_DIR P1DIR

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

void ds18b20_start_conversion(void)
{
    reset_18B20();
    send_18B20(0xcc);   //send CCH,Skip ROM command
    send_18B20(0x44);
}

float ds18b20_read_temperature(void)
{
    unsigned int temp;
    int signedTemp;

    reset_18B20();
    send_18B20(0xcc);   //send CCH,Skip ROM command
    send_18B20(0xbe);

    temp = read_18B20();

    /*
     * read_18B20() returns the signed DS18B20 value shifted right once.
     * Restore the sign bit before converting the 1/8-degree result.
     */
    if (temp & 0x4000U)
        temp |= 0x8000U;
    signedTemp = (int)temp;

    return((float)signedTemp/8.0f);
}

float get_temp(void)
{
    ds18b20_start_conversion();

    // Blocking compatibility path. Temperature mode uses LPM3 instead.
    delay_us(750000UL);

    return ds18b20_read_temperature();
}

void reset_18B20(void)
{
    ONE_WIRE_DIR |=ONE_WIRE_PIN;
    ONE_WIRE_OUT &= ~ONE_WIRE_PIN;
    __delay_cycles(500);
    ONE_WIRE_OUT |=ONE_WIRE_PIN;
    ONE_WIRE_DIR &= ~ONE_WIRE_PIN;
    __delay_cycles(500);
}

void send_18B20(char data)
{
    char i;

    for(i=8;i>0;i--)
    {
    	ONE_WIRE_DIR |=ONE_WIRE_PIN;
        ONE_WIRE_OUT &= ~ONE_WIRE_PIN;
        __delay_cycles(2);
        if(data & 0x01)
        {
            ONE_WIRE_OUT |= ONE_WIRE_PIN;
        }
        __delay_cycles(60);
        ONE_WIRE_OUT |= ONE_WIRE_PIN;
        ONE_WIRE_DIR &= ~ONE_WIRE_PIN;
        data >>=1;
    }
}

unsigned int read_18B20()
{
    char i;
    unsigned int data=0;

    for(i=16;i>0;i--)
    {
    	ONE_WIRE_DIR |= ONE_WIRE_PIN;
        ONE_WIRE_OUT &= ~ONE_WIRE_PIN;
        __delay_cycles(2);
        ONE_WIRE_OUT |=ONE_WIRE_PIN;
        ONE_WIRE_DIR &= ~ONE_WIRE_PIN;
        __delay_cycles(8);
        if(ONE_WIRE_IN & ONE_WIRE_PIN)
        {
            data |=0x8000;
        }
        data>>=1;
        __delay_cycles(120);
    }
    return(data);
}
