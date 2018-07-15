/*
 * CANaerospace usage example for STM32 with FreeRTOS.
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#ifndef SENSORS_H_
#define SENSORS_H_

int sensInit(void);
float sensReadTemperatureKelvin(void);
float sensReadVoltage(void);

#endif
