#ifndef DS18B20_H_
#define DS18B20_H_

#include <stdint.h>

typedef enum
{
    DS18B20_STATUS_OK = 0,
    DS18B20_STATUS_NO_PRESENCE,
    DS18B20_STATUS_CRC_MISMATCH
} Ds18b20Status;

Ds18b20Status get_temp(float *temperatureC);
Ds18b20Status ds18b20_start_conversion(void);
Ds18b20Status ds18b20_read_temperature(float *temperatureC);
void delay_us(unsigned long microseconds);
unsigned char reset_18B20(void);
void send_18B20(uint8_t data);

#endif /* DS18B20_H_ */
