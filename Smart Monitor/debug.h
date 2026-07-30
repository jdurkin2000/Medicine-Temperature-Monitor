void uartSendChar(char c);

void uartSendString(const char* str);

// Convert an integer to ASCII and transmit via UART
void uartSendInt(long n);

// Sends a float to UART given a specific number of decimal places (e.g., 2 or 3)
void uartSendFloat(float val, unsigned char decimals);