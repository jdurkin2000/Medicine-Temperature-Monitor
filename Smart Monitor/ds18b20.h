#ifndef DS18B20_H_
#define DS18B20_H_

float get_temp(void);
void delay_us(unsigned long microseconds);
void reset_18B20(void);
void send_18B20(char data);
unsigned int read_18B20(void);

#endif /* DS18B20_H_ */
