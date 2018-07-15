/*
 * Service Port
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <misc.h>
#include <stm32f10x_usart.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_gpio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "srvport.h"

#define SRVPORT_BAUDRATE                 115200
#define SRVPORT_TX_QUEUE_LEN             512
#define SRVPORT_RX_QUEUE_LEN             512
#define SRVPORT_NVIC_PREEMPTION_PRIORITY 15
#define SRVPORT_ECHO                     1

static xSemaphoreHandle _mutex;

static uint8_t _buf_rx[SRVPORT_RX_QUEUE_LEN];
static uint8_t _buf_tx[SRVPORT_TX_QUEUE_LEN];

typedef struct
{
    uint8_t* const pbuf;
    const int bufsize;
    int in;
    int out;
    int len;
} Fifo;

static Fifo _fifo_rx = { _buf_rx, SRVPORT_RX_QUEUE_LEN, 0, 0, 0 };
static Fifo _fifo_tx = { _buf_tx, SRVPORT_TX_QUEUE_LEN, 0, 0, 0 };

static inline void _fifoPush(Fifo* pfifo, uint8_t data)
{
    __disable_irq();
    pfifo->pbuf[pfifo->in++] = data;
    if (pfifo->in >= pfifo->bufsize)
        pfifo->in = 0;

    if (pfifo->len >= pfifo->bufsize)
    {
        pfifo->out++;
        if (pfifo->out >= pfifo->bufsize)
            pfifo->out = 0;
    }
    else
        pfifo->len++;

    __enable_irq();
}

static inline int _fifoPop(Fifo* pfifo)
{
    register int retval = -1;
    __disable_irq();
    if (pfifo->len <= 0)
        goto _exit;

    pfifo->len--;
    retval = pfifo->pbuf[pfifo->out++];
    if (pfifo->out >= pfifo->bufsize)
        pfifo->out = 0;

    _exit:
    __enable_irq();
    return retval;
}

static inline int _fifoLen(const Fifo* pfifo)
{
    register int retval = -1;
    __disable_irq();
    retval = pfifo->len;
    __enable_irq();
    return retval;
}

static inline int _fifoFree(const Fifo* pfifo)
{
    register int retval = -1;
    __disable_irq();
    retval = pfifo->bufsize - pfifo->len;
    __enable_irq();
    return retval;
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART2);
        _fifoPush(&_fifo_rx, byte);
#if SRVPORT_ECHO
        _fifoPush(&_fifo_tx, byte);
        USART2->CR1 |= USART_CR1_TXEIE;
#endif
    }

    if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET)
    {
        register int next = _fifoPop(&_fifo_tx);
        if (next >= 0)
            USART_SendData(USART2, next);
        else
            USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
    }
}

static void _putCharPoll(char ch)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == 0)
    { }
    USART_SendData(USART2, ch);
}

int srvportInit(void)
{
    USART_InitTypeDef usart_init;
    NVIC_InitTypeDef nvic_init;
    GPIO_InitTypeDef gpio_init;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    GPIO_PinRemapConfig(GPIO_Remap_USART2, ENABLE);
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_Pin = GPIO_Pin_6;
    gpio_init.GPIO_Mode = GPIO_Mode_IPD;               // Pull-Down used to detect when terminal is attached
    GPIO_Init(GPIOD, &gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_5;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &gpio_init);

    USART_DeInit(USART2);
    USART_StructInit(&usart_init);
    usart_init.USART_BaudRate = SRVPORT_BAUDRATE;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART2, &usart_init);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    nvic_init.NVIC_IRQChannelPreemptionPriority = SRVPORT_NVIC_PREEMPTION_PRIORITY;
    nvic_init.NVIC_IRQChannelSubPriority = 0;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    nvic_init.NVIC_IRQChannel = USART2_IRQn;
    NVIC_Init(&nvic_init);

    USART_Cmd(USART2, ENABLE);

    _mutex = xSemaphoreCreateMutex();
    if (_mutex == NULL)
        return -1;
    return 0;
}

int srvportIsAttached(void)
{
    return GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_6);
}

int srvportWrite(const uint8_t* pdata, int len)
{
    if (xSemaphoreTake(_mutex, portMAX_DELAY))
    {
        register int written = 0;
        while (len-- > 0 && _fifoFree(&_fifo_tx) > 0)
        {
            _fifoPush(&_fifo_tx, *pdata++);
            written++;
        }

        __disable_irq();
        USART2->CR1 |= USART_CR1_TXEIE;
        __enable_irq();

        xSemaphoreGive(_mutex);
        return written;
    }
    return -1;
}

int srvportRead(uint8_t* pdata, int maxlen)
{
    if (xSemaphoreTake(_mutex, portMAX_DELAY))
    {
        register int count = 0, tmp = -1;
        while (maxlen-- > 0)
        {
            tmp = _fifoPop(&_fifo_rx);
            if (tmp < 0)
                break;
            *pdata++ = (uint8_t)tmp;
            count++;
        }

        xSemaphoreGive(_mutex);
        return count;
    }
    return -1;
}

int srvportGetChar(void)
{
    return _fifoPop(&_fifo_rx);
}

void srvportFlush(void)
{
    if (xSemaphoreTake(_mutex, portMAX_DELAY))
    {
        while (_fifoLen(&_fifo_tx))
            vTaskDelay(1);
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET)
            vTaskDelay(1);
        xSemaphoreGive(_mutex);
    }
}

void srvportDieWithHonour(const char* const* compound_message)
{
    __disable_irq();
    for (;;)
    {
        _putCharPoll('\n');
        int m = 0;
        while (compound_message[m])
        {
            int c = 0;
            while (compound_message[m][c])
            {
                _putCharPoll(compound_message[m][c]);
                c++;
            }
            m++;
        }
        _putCharPoll('\n');
    }
}
