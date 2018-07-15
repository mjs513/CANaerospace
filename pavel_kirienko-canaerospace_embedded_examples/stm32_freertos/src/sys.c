/*
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <misc.h>
#include <stm32f10x.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include <FreeRTOS.h>
#include <task.h>
#include "srvport/srvport.h"
#include "sys.h"
#include "util.h"

unsigned long ulRunTimeStatsClock = 0;

static uint64_t _sys_time_ms = 0;

void sysInit()
{
    // http://www.freertos.org/RTOS-Cortex-M3-M4.html
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    // CAN 1
    GPIO_PinRemapConfig(GPIO_Remap2_CAN1, ENABLE);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOD, &GPIO_InitStructure);            // RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);            // TX

    // CAN2
    GPIO_PinRemapConfig(GPIO_Remap_CAN2, ENABLE);
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);            // RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);            // TX

    // LEDs
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Idle task monitor
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // Service stuff
    ASSERT_ALWAYS(srvportInit() == 0);
    ASSERT_ALWAYS((1000 % configTICK_RATE_HZ) == 0);
}

void sysLedSet(int value)
{
    *(value ? &GPIOB->BSRR : &GPIOB->BRR) = GPIO_Pin_9;
}

uint64_t sysTimestampMicros(void)
{
    register uint64_t ret = 0;
    vPortEnterCritical();
    ret = _sys_time_ms;
    vPortExitCritical();
    return ret * 1000ul;
}

void vApplicationIdleHook(void)
{
    GPIOD->BRR = GPIO_Pin_10;
    __asm volatile("wfi");
    GPIOD->BSRR = GPIO_Pin_10;
}

void vApplicationTickHook(void)
{
    _sys_time_ms += portTICK_RATE_MS;
}

void vApplicationMallocFailedHook(void)
{
    const char* pstr[] = { "OUT OF MEMORY ", NULL };
    srvportDieWithHonour(pstr);
}

void vApplicationStackOverflowHook(xTaskHandle xTask, signed portCHAR *pcTaskName)
{
    (void)xTask;
    const char* pstr[] = { "STACK OVERFLOW ", (const char*)pcTaskName, NULL };
    srvportDieWithHonour(pstr);
}

void assert_failed(const char* file, int line)
{
    char buf[13];
    const char* pstr[] = { "ASSERT ", file, ":", itoa(line, buf), NULL };
    srvportDieWithHonour(pstr);
}
