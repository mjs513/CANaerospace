/*
 * Timing stuff
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include "can_driver.h"
#include "internal.h"

#if !CAN_TIMER_EMULATED

#if CAN_CHIBIOS
#   include <hal.h>
#else
#   include <misc.h>
#   include <stm32f10x_rcc.h>
#endif
#include <stm32f10x_tim.h>
#include <stdint.h>

#define GLUE2_(A, B) A##B
#define GLUE2(A, B) GLUE2_(A, B)
#define GLUE3_(A, B, C) A##B##C
#define GLUE3(A, B, C) GLUE3_(A, B, C)

// macro instantiation:
#define RCC_APB1ENR_TIMxEN  GLUE3(RCC_APB1ENR_TIM, CAN_TIMER_NUMBER, EN)
#define TIMx                GLUE2(TIM, CAN_TIMER_NUMBER)
#define TIMx_IRQn           GLUE3(TIM, CAN_TIMER_NUMBER, _IRQn)
#define TIMx_IRQHandler     GLUE3(TIM, CAN_TIMER_NUMBER, _IRQHandler)

#define TIMER_USEC_PER_TICK 16
#define TIMER_CLOCK_HZ       (1000000 / TIMER_USEC_PER_TICK)

CAN_IRQ_HANDLER(TIMx_IRQHandler)
{
    CAN_IRQ_PROLOGUE();

    if (TIM_GetITStatus(TIMx, TIM_FLAG_CC1) != RESET)
    {
        TIM_ClearFlag(TIMx, TIM_FLAG_CC1);
        canTimerIrq(0);
    }
    if (TIM_GetITStatus(TIMx, TIM_FLAG_CC2) != RESET)
    {
        TIM_ClearFlag(TIMx, TIM_FLAG_CC2);
        canTimerIrq(1);
    }

    CAN_IRQ_EPILOGUE();
}

int canTimerInit(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIMxEN;
    TIM_DeInit(TIMx);

    uint32_t tim_clk = 0;
#if CAN_CHIBIOS
    tim_clk = STM32_TIMCLK1;  // Timers 2, 3, 4, 5, 6, 7
#else
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    if (clocks.HCLK_Frequency == clocks.PCLK1_Frequency)    // if APB1 prescaler == 1
        tim_clk = clocks.PCLK1_Frequency;
    else
        tim_clk = clocks.PCLK1_Frequency * 2;
#endif // chibios

    TIM_TimeBaseInitTypeDef tim_base_init;
    TIM_TimeBaseStructInit(&tim_base_init);
    tim_base_init.TIM_Period = 0xFFFF;
    tim_base_init.TIM_Prescaler = tim_clk / TIMER_CLOCK_HZ - 1;
    tim_base_init.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIMx, &tim_base_init);

    TIM_OC1PreloadConfig(TIMx, TIM_OCPreload_Disable); // OC will update immediately
    TIM_OC2PreloadConfig(TIMx, TIM_OCPreload_Disable);
    TIM_Cmd(TIMx, ENABLE);

#if CAN_CHIBIOS
    nvicEnableVector(TIMx_IRQn,  CORTEX_PRIORITY_MASK(CAN_IRQ_PRIORITY));
#else
    NVIC_InitTypeDef nvic_init_struct;
    nvic_init_struct.NVIC_IRQChannelPreemptionPriority = CAN_IRQ_PRIORITY;
    nvic_init_struct.NVIC_IRQChannelSubPriority = CAN_IRQ_SUBPRIORITY;
    nvic_init_struct.NVIC_IRQChannelCmd = ENABLE;
    nvic_init_struct.NVIC_IRQChannel = TIMx_IRQn;
    NVIC_Init(&nvic_init_struct);
#endif
    return 0;
}

void canTimerDeinit(void)
{
    TIM_DeInit(TIMx);
    RCC->APB1ENR &= ~RCC_APB1ENR_TIMxEN;
}

void canTimerSet(int iface, int usec)
{
    if (iface != 0 && iface != 1)
        return;

    TIM_ITConfig(TIMx, (iface == 0) ? TIM_IT_CC1 : TIM_IT_CC2, DISABLE);

    int clocks = usec / TIMER_USEC_PER_TICK;
    if (clocks > 0xFFFF)
        clocks = 0xFFFF;
    if (clocks < 2)
        clocks = 2;

    TIM_OCInitTypeDef tim_oc_init;
    TIM_OCStructInit(&tim_oc_init);
    tim_oc_init.TIM_OCMode = TIM_OCMode_Timing;
    tim_oc_init.TIM_Pulse = TIMx->CNT + clocks;
    ((iface == 0) ? TIM_OC1Init : TIM_OC2Init)(TIMx, &tim_oc_init);

    TIM_ClearFlag(TIMx, (iface == 0) ? TIM_FLAG_CC1 : TIM_FLAG_CC2);
    TIM_ITConfig(TIMx, (iface == 0) ? TIM_IT_CC1 : TIM_IT_CC2, ENABLE);
}

void canTimerStop(int iface)
{
    if (iface != 0 && iface != 1)
        return;
    TIM_ITConfig(TIMx, (iface == 0) ? TIM_IT_CC1 : TIM_IT_CC2, DISABLE);
}

#else  // if CAN_TIMER_EMULATED is actually true

static int _remained_usec[CAN_IFACE_COUNT];

int canTimerInit(void)
{
    canTimerDeinit();
    return 0;
}

void canTimerDeinit(void)
{
    for (int i = 0; i < CAN_IFACE_COUNT; i++)
        _remained_usec[i] = -1;
}

void canTimerSet(int iface, int usec)
{
    if (iface < 0 || iface >= CAN_IFACE_COUNT)
        return;
    _remained_usec[iface] = usec;
}

void canTimerStop(int iface)
{
    if (iface < 0 || iface >= CAN_IFACE_COUNT)
        return;
    _remained_usec[iface] = -1;
}

void canTimerEmulIncrementIrq(int usec)
{
    for (int i = 0; i < CAN_IFACE_COUNT; i++)
    {
        if (_remained_usec[i] < 0)
            continue;
        _remained_usec[i] -= usec;
        if (_remained_usec[i] <= 0)
            canTimerIrq(i);
    }
}

#endif
