#include <driverlib.h>
#include "debug.h"

void uartSendChar(char c) {
    while (!(UCA1IFG & UCTXIFG)); // Wait for TX buffer
    UCA1TXBUF = c;
}

void uartSendString(const char* str) {
    while (*str) {
        uartSendChar(*str++);
    }
}

// Convert an integer to ASCII and transmit via UART
void uartSendInt(long n) {
    char buf[12];
    int i = 0;

    if (n < 0) {
        uartSendChar('-');
        n = -n;
    }
    if (n == 0) {
        uartSendChar('0');
        return;
    }

    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }

    // Print array backwards
    while (i > 0) {
        uartSendChar(buf[--i]);
    }
}

// Sends a float to UART given a specific number of decimal places (e.g., 2 or 3)
void uartSendFloat(float val, unsigned char decimals) {
    // Handle negative numbers
    if (val < 0) {
        uartSendChar('-');
        val = -val;
    }

    // Extract integer portion
    long intPart = (long)val;
    uartSendInt(intPart);

    // Print decimal point
    uartSendChar('.');

    // Extract fraction part by scaling up by powers of 10
    float fraction = val - (float)intPart;
    unsigned char i;
    for (i = 0; i < decimals; i++) {
        fraction *= 10.0f;
    }

    long fracPart = (long)(fraction + 0.5f); // Round to nearest integer

    // Handle padded zeroes (e.g., if float is 3.004, fractional part is 4, needs leading zeroes)
    long temp = fracPart;
    unsigned char digits = 0;
    while (temp > 0) {
        digits++;
        temp /= 10;
    }
    if (fracPart == 0) digits = 1;

    // Pad leading zeroes after decimal point
    while (digits < decimals) {
        uartSendChar('0');
        digits++;
    }

    uartSendInt(fracPart);
}