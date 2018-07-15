/*
 * CANaerospace usage example for STM32 with FreeRTOS.
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include "sensors.h"

int sensInit(void)
{
    return 0;
}

float sensReadTemperatureKelvin(void)
{
    // TODO: obtain temperature measurements from embedded temperature sensor
    return 1.23;
}

float sensReadVoltage(void)
{
    // TODO: measure something :)
    return 4.56;
}
