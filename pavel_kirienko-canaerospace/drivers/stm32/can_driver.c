/*
 * STM32 CAN driver for the CANaerospace library
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if CAN_FREERTOS
#   include <FreeRTOS.h>
#   include <queue.h>
#endif

#if CAN_CHIBIOS
#   include <ch.h>
#   include <hal.h>
#else
#   include <misc.h>
#   include <stm32f10x_rcc.h>
#endif

#include <stm32f10x_can.h>
#include "can_driver.h"
#include "internal.h"

#if !(defined(STM32F10X_CL) || defined(STM32F2XX) || defined(STM32F4XX))

// IRQ numbers
#define CAN1_RX0_IRQn USB_LP_CAN1_RX0_IRQn
#define CAN1_TX_IRQn USB_HP_CAN1_TX_IRQn

// IRQ vectors
#if !defined(CAN1_RX0_IRQHandler) || !defined(CAN1_TX_IRQHandler)
#   define CAN1_TX_IRQHandler   USB_HP_CAN1_TX_IRQHandler
#   define CAN1_RX0_IRQHandler  USB_LP_CAN1_RX0_IRQHandler
#endif

#endif

static unsigned int _error_mask[2] = {0, 0};
static unsigned int _frame_tx_timeout_usec = 0;

typedef struct
{
    CanasCanFrame frame;
    uint8_t iface;
} FifoEntry;

typedef struct
{
    FifoEntry* const pbuf;
    const int bufsize;
    int in;
    int out;
    int len;
} Fifo;

#if CAN_IFACE_COUNT == 1
#   define CAN_RX_QUEUE_LEN_TOTAL CAN_RX_QUEUE_LEN
#elif CAN_IFACE_COUNT == 2
#   define CAN_RX_QUEUE_LEN_TOTAL (CAN_RX_QUEUE_LEN * 2)
#else
#   error "CAN_IFACE_COUNT must be 1 or 2"
#endif

#if CAN_FREERTOS

static xQueueHandle _queue_rx = NULL;

#else

static FifoEntry _buff_rx[CAN_RX_QUEUE_LEN_TOTAL];
static Fifo _fifo_rx = { _buff_rx, CAN_RX_QUEUE_LEN_TOTAL, 0, 0, 0 };

#endif

#if CAN_CHIBIOS
static EVENTSOURCE_DECL(_on_rx);
#endif

static FifoEntry _buff_tx[CAN_IFACE_COUNT][CAN_TX_QUEUE_LEN];
static Fifo _fifo_tx[CAN_IFACE_COUNT] =
{
    { _buff_tx[0], CAN_TX_QUEUE_LEN, 0, 0, 0 }
#if CAN_IFACE_COUNT > 1
    ,
    { _buff_tx[1], CAN_TX_QUEUE_LEN, 0, 0, 0 }
#endif
};

/* Software FIFO */

static inline void _fifoPush(Fifo* pfifo, const FifoEntry* pframe, bool* overflow)
{
    pfifo->pbuf[pfifo->in++] = *pframe;
    if (pfifo->in >= pfifo->bufsize)
        pfifo->in = 0;
    if (pfifo->len >= pfifo->bufsize)
    {
        *overflow = true;
        pfifo->out++;
        if (pfifo->out >= pfifo->bufsize)
            pfifo->out = 0;
    }
    else
    {
        *overflow = false;
        pfifo->len++;
    }
}

static inline int _fifoPop(Fifo* pfifo, FifoEntry* pframe)
{
    if (pfifo->len <= 0)
        return 0;
    pfifo->len--;
    if (pframe)
        *pframe = pfifo->pbuf[pfifo->out++];
    if (pfifo->out >= pfifo->bufsize)
        pfifo->out = 0;
    return 1;
}

static inline void _frameSpl2Canas(const CanRxMsg* pspl, CanasCanFrame* pcanas)
{
    memset(pcanas, 0, sizeof(*pcanas));
    memcpy(pcanas->data, pspl->Data, pspl->DLC);
    pcanas->dlc = pspl->DLC;
    if (pspl->IDE == CAN_Id_Standard)
    {
        pcanas->id = pspl->StdId & CANAS_CAN_MASK_STDID;
    }
    else
    {
        pcanas->id = pspl->ExtId & CANAS_CAN_MASK_EXTID;
        pcanas->id |= CANAS_CAN_FLAG_EFF;
    }
    if (pspl->RTR == CAN_RTR_Remote)
        pcanas->id |= CANAS_CAN_FLAG_RTR;
}

static inline void _frameCanas2Spl(const CanasCanFrame* pcanas, CanTxMsg* pspl)
{
    memset(pspl, 0, sizeof(*pspl));
    memcpy(pspl->Data, pcanas->data, pcanas->dlc);
    pspl->DLC = pcanas->dlc;
    if (pcanas->id & CANAS_CAN_FLAG_EFF)
    {
        pspl->ExtId = pcanas->id & CANAS_CAN_MASK_EXTID;
        pspl->IDE = CAN_Id_Extended;
    }
    else
    {
        pspl->StdId = pcanas->id & CANAS_CAN_MASK_STDID;
        pspl->IDE = CAN_Id_Standard;
    }
    pspl->RTR = (pcanas->id & CANAS_CAN_FLAG_RTR) ? CAN_RTR_Remote : CAN_RTR_Data;
}

/* Error monitoring */

static inline void _setErrors(int iface, unsigned int mask)
{
    if (iface == 0 || iface == 1)
        _error_mask[iface] |= mask;
}

static void _pollHardwareErrors(CAN_TypeDef* CANx, int iface)
{
    unsigned int errmask = 0;

    if (CAN_GetFlagStatus(CANx, CAN_FLAG_BOF))   // There is no way to clear this flag by software
        errmask |= CAN_ERRFLAG_BUS_OFF;

    if (CAN_GetFlagStatus(CANx, CAN_FLAG_LEC))
    {
        errmask |= CAN_ERRFLAG_HARDWARE;
        CAN_ClearFlag(CANx, CAN_FLAG_LEC);
    }
    // RX FIFO overflow flag doesn't work - it's always cleared when an overflow occurs
    if (CANx->RF0R & CAN_RF0R_FOVR0 || CANx->RF1R & CAN_RF1R_FOVR1)
    {
        errmask |= CAN_ERRFLAG_RX_LOST;
        CAN_ClearFlag(CANx, CAN_FLAG_FOV0);
        CAN_ClearFlag(CANx, CAN_FLAG_FOV1);
    }
    __disable_irq();
    _setErrors(iface, errmask);
    __enable_irq();
}

/* Hardware control */

#define NUM_OF_MAILBOXES 3

#define HAS_EMPTY_MAILBOX(CANx) (!!((CANx)->TSR & CAN_TSR_TME))

#define HAS_PENDING_MAILBOX(CANx) \
    (!(((CANx)->TSR & CAN_TSR_TME0) && ((CANx)->TSR & CAN_TSR_TME1) && ((CANx)->TSR & CAN_TSR_TME2)))

static inline void _tryTransmit(int iface) __attribute__((always_inline));
static inline void _tryTransmit(int iface)
{
    CanTxMsg frame_spl;
    FifoEntry fe;
    if (!HAS_EMPTY_MAILBOX(((iface == 0) ? CAN1 : CAN2)))
        return;
    if (_fifoPop(&_fifo_tx[iface], &fe))
    {
        _frameCanas2Spl(&fe.frame, &frame_spl);
        CAN_Transmit((iface == 0) ? CAN1 : CAN2, &frame_spl);
        canTimerSet(iface, _frame_tx_timeout_usec * NUM_OF_MAILBOXES);
    }
}

static inline void _abortPendingTransmissions(int iface)
{
    ((iface == 0) ? CAN1 : CAN2)->TSR |= CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2;
}

void canTimerIrq(int iface)
{
    // It's not necessary to perform further actions here, because we will get TX IRQ after that.
    _abortPendingTransmissions(iface);
    _setErrors(iface, CAN_ERRFLAG_TX_TIMEOUT);
}

static inline void _genericTxIrqHandler(CAN_TypeDef* CANx, int iface) __attribute__((always_inline));
static inline void _genericTxIrqHandler(CAN_TypeDef* CANx, int iface)
{
    register uint32_t tsr = CANx->TSR;
    if (tsr & CAN_TSR_RQCP0)
    {
        tsr |= CAN_TSR_RQCP0;
        _tryTransmit(iface);
    }
    if (tsr & CAN_TSR_RQCP1)
    {
        tsr |= CAN_TSR_RQCP1;
        _tryTransmit(iface);
    }
    if (tsr & CAN_TSR_RQCP2)
    {
        tsr |= CAN_TSR_RQCP2;
        _tryTransmit(iface);
    }
    CANx->TSR = tsr;
    if (!HAS_PENDING_MAILBOX(CANx))    // Nothing left to transmit, so timer is no use anymore
        canTimerStop(iface);
}

CAN_IRQ_HANDLER(CAN1_TX_IRQHandler)
{
    CAN_IRQ_PROLOGUE();
    _genericTxIrqHandler(CAN1, 0);
    CAN_IRQ_EPILOGUE();
}

#if CAN_IFACE_COUNT > 1
CAN_IRQ_HANDLER(CAN2_TX_IRQHandler)
{
    CAN_IRQ_PROLOGUE();
    _genericTxIrqHandler(CAN2, 1);
    CAN_IRQ_EPILOGUE();
}
#endif

static inline void _genericRxIrqHandler(CAN_TypeDef* CANx, uint8_t hwfifo, int iface)
{
#if CAN_FREERTOS
    portBASE_TYPE task_woken = 0;
#endif
    if (CAN_GetFlagStatus(CANx, (hwfifo == CAN_FIFO0) ? CAN_FLAG_FMP0 : CAN_FLAG_FMP1))
    {
        CanRxMsg frame_spl;
        CAN_Receive(CANx, hwfifo, &frame_spl);

        FifoEntry fifo_entry;
        fifo_entry.iface = iface;
        _frameSpl2Canas(&frame_spl, &fifo_entry.frame);

        bool overflow = 0;
#if CAN_FREERTOS
        if (xQueueIsQueueFullFromISR(_queue_rx))
        {
            FifoEntry waste;
            xQueueReceiveFromISR(_queue_rx, &waste, &task_woken);  // Queue is full, so we have to kill the last entry
            overflow = true;
        }
        task_woken = 0;
        xQueueSendFromISR(_queue_rx, &fifo_entry, &task_woken);
#else
        _fifoPush(&_fifo_rx, &fifo_entry, &overflow);
#endif
#if CAN_CHIBIOS
        chSysLockFromIsr();
        chEvtBroadcastFlagsI(&_on_rx, ALL_EVENTS);
        chSysUnlockFromIsr();
#endif
        if (overflow)
            _setErrors(iface, CAN_ERRFLAG_RX_OVERFLOW);
    }
#if CAN_FREERTOS
    portEND_SWITCHING_ISR(task_woken);
#endif
}

CAN_IRQ_HANDLER(CAN1_RX0_IRQHandler)
{
    CAN_IRQ_PROLOGUE();
    _genericRxIrqHandler(CAN1, CAN_FIFO0, 0);
    CAN_IRQ_EPILOGUE();
}

CAN_IRQ_HANDLER(CAN1_RX1_IRQHandler)
{
    CAN_IRQ_PROLOGUE();
    _genericRxIrqHandler(CAN1, CAN_FIFO1, 0);
    CAN_IRQ_EPILOGUE();
}

#if CAN_IFACE_COUNT > 1
CAN_IRQ_HANDLER(CAN2_RX0_IRQHandler)
{
    CAN_IRQ_PROLOGUE();
    _genericRxIrqHandler(CAN2, CAN_FIFO0, 1);
    CAN_IRQ_EPILOGUE();
}

CAN_IRQ_HANDLER(CAN2_RX1_IRQHandler)
{
    CAN_IRQ_PROLOGUE();
    _genericRxIrqHandler(CAN2, CAN_FIFO1, 1);
    CAN_IRQ_EPILOGUE();
}
#endif

static void _initInterrupts(void)
{
#if CAN_CHIBIOS
    nvicEnableVector(CAN1_TX_IRQn,  CORTEX_PRIORITY_MASK(CAN_IRQ_PRIORITY));
    nvicEnableVector(CAN1_RX0_IRQn, CORTEX_PRIORITY_MASK(CAN_IRQ_PRIORITY));
    nvicEnableVector(CAN1_RX1_IRQn, CORTEX_PRIORITY_MASK(CAN_IRQ_PRIORITY));
#if CAN_IFACE_COUNT > 1
    nvicEnableVector(CAN2_TX_IRQn,  CORTEX_PRIORITY_MASK(CAN_IRQ_PRIORITY));
    nvicEnableVector(CAN2_RX0_IRQn, CORTEX_PRIORITY_MASK(CAN_IRQ_PRIORITY));
    nvicEnableVector(CAN2_RX1_IRQn, CORTEX_PRIORITY_MASK(CAN_IRQ_PRIORITY));
#endif
#else
    NVIC_InitTypeDef nvic_init_struct;
    memset(&nvic_init_struct, 0, sizeof(nvic_init_struct));
    nvic_init_struct.NVIC_IRQChannelPreemptionPriority = CAN_IRQ_PRIORITY;
    nvic_init_struct.NVIC_IRQChannelSubPriority = CAN_IRQ_SUBPRIORITY;
    nvic_init_struct.NVIC_IRQChannelCmd = ENABLE;

    nvic_init_struct.NVIC_IRQChannel = CAN1_RX0_IRQn;
    NVIC_Init(&nvic_init_struct);
    nvic_init_struct.NVIC_IRQChannel = CAN1_RX1_IRQn;
    NVIC_Init(&nvic_init_struct);
    nvic_init_struct.NVIC_IRQChannel = CAN1_TX_IRQn;
    NVIC_Init(&nvic_init_struct);
#if CAN_IFACE_COUNT > 1
    nvic_init_struct.NVIC_IRQChannel = CAN2_RX0_IRQn;
    NVIC_Init(&nvic_init_struct);
    nvic_init_struct.NVIC_IRQChannel = CAN2_RX1_IRQn;
    NVIC_Init(&nvic_init_struct);
    nvic_init_struct.NVIC_IRQChannel = CAN2_TX_IRQn;
    NVIC_Init(&nvic_init_struct);
#endif
#endif

    uint32_t irq_mask = CAN_IT_TME | CAN_IT_FMP0 | CAN_IT_FMP1;
    CAN1->IER = irq_mask;
#if CAN_IFACE_COUNT > 1
    CAN2->IER = irq_mask;
#endif
}

static int _configureTimings(int target_bitrate, CAN_InitTypeDef* output)
{
    /*
     *   BITRATE = 1 / (PRESCALER * (1 / PCLK) * (1 + BS1 + BS2))
     *   BITRATE = PCLK / (PRESCALER * (1 + BS1 + BS2))
     * let:
     *   BS = 1 + BS1 + BS2
     *   PRESCALER_BS = PRESCALER * BS
     * ==>
     *   PRESCALER_BS = PCLK / BITRATE
     */
    if (target_bitrate < 20000 || target_bitrate > 2000000)
        return -1;

#if CAN_CHIBIOS
    const int pclk = STM32_PCLK1;
#else
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    const int pclk = clocks.PCLK1_Frequency;
#endif

    const int prescaler_bs = pclk / target_bitrate;

    // Initial guess:
    int bs1 = 10;
    int bs2 = 5;
    int prescaler = -1;

    while (1)
    {
        prescaler = prescaler_bs / (1 + bs1 + bs2);
        // Check result:
        if (IS_CAN_PRESCALER(prescaler))
        {
            const int current_bitrate = pclk / (prescaler * (1 + bs1 + bs2));
            if (current_bitrate == target_bitrate)
                break;
        }

        if (bs1 > bs2)
            bs1--;
        else
            bs2--;
        if (bs1 <= 0 || bs2 <= 0)
            return -2;
    }

    if (!IS_CAN_PRESCALER(prescaler))
        return -3;

    output->CAN_SJW = 1;
    output->CAN_BS1 = bs1 - 1;
    output->CAN_BS2 = bs2 - 1;
    output->CAN_Prescaler = prescaler; // Note that SPL will decrement CAN_Prescaler by one
    return 0;
}

/* Interface */

int canInit(int bitrate, unsigned int tx_timeout_usec)
{
    int result = -1;

    if (tx_timeout_usec <= 125)    // Min message time for 1Mbps, std id.
        return -1;
    _frame_tx_timeout_usec = tx_timeout_usec;

    /*
     * Enable the CAN cells and the timer; selftest
     */
    if (canTimerInit() != 0)
        goto leave_error;

    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    if (canSelftest(CAN1) != 0)
        goto leave_error;
    CAN_DeInit(CAN1);

#if CAN_IFACE_COUNT > 1
    RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;
    if (canSelftest(CAN2) != 0)
        goto leave_error;
    CAN_DeInit(CAN2);
#endif

    /*
     * CAN configuration (same parameters for all interfaces)
     */
    CAN_InitTypeDef can_init_struct;
    CAN_StructInit(&can_init_struct);
    can_init_struct.CAN_ABOM = ENABLE;           // Automatic Bus-Off recovery
    can_init_struct.CAN_AWUM = ENABLE;
    can_init_struct.CAN_NART = DISABLE;
    can_init_struct.CAN_RFLM = DISABLE;
    can_init_struct.CAN_TTCM = DISABLE;
    can_init_struct.CAN_TXFP = ENABLE;           // Transmit in order
    can_init_struct.CAN_Mode = CAN_Mode_Normal;

    result = _configureTimings(bitrate, &can_init_struct);
    if (result != 0)
        goto leave_error;

    if (CAN_Init(CAN1, &can_init_struct) != CAN_InitStatus_Success)
        goto leave_error;
#if CAN_IFACE_COUNT > 1
    if (CAN_Init(CAN2, &can_init_struct) != CAN_InitStatus_Success)
        goto leave_error;
#endif

    _initInterrupts();
    result = canFilterInit();
    if (result != 0)
        goto leave_error;

    /*
     * OS-specific initialization
     */
#if CAN_FREERTOS
    if (_queue_rx == NULL)
        _queue_rx = xQueueCreate(CAN_RX_QUEUE_LEN_TOTAL, sizeof(FifoEntry));
    if (_queue_rx == NULL)
        goto leave_error;
#endif
#if CAN_CHIBIOS
    chEvtInit(&_on_rx);
#endif

    return 0;

leave_error:
    CAN_DeInit(CAN1);
    RCC->APB1ENR &= ~RCC_APB1ENR_CAN1EN;
#if CAN_IFACE_COUNT > 1
    CAN_DeInit(CAN2);
    RCC->APB1ENR &= ~RCC_APB1ENR_CAN2EN;
#endif
    canTimerDeinit();
    return (result < 0) ? result : -1;
}

unsigned int canYieldErrors(int iface)
{
    if (iface < 0 || iface >= CAN_IFACE_COUNT)
        return -1;

    _pollHardwareErrors((iface == 0) ? CAN1 : CAN2, iface);

    __disable_irq();
    const unsigned int errmask = _error_mask[iface];
    _error_mask[iface] = 0;
    __enable_irq();

    return errmask;
}

int canSend(int iface, const CanasCanFrame* pframe)
{
    if ((iface < 0 || iface >= CAN_IFACE_COUNT) || pframe == NULL)
        return -1;

    FifoEntry fe;
    fe.iface = -1;   // Not used really
    fe.frame = *pframe;

    __disable_irq();
    bool overflow = false;
    _fifoPush(&_fifo_tx[iface], &fe, &overflow);
    _tryTransmit(iface);                          // Will transmit if there is free transmit mailbox
    if (overflow)
    {
        _setErrors(iface, CAN_ERRFLAG_TX_OVERFLOW);
        _abortPendingTransmissions(iface);        // Objective is to transmit the newer data faster
    }
    __enable_irq();
    return 1;
}

#if CAN_FREERTOS

int canReceive(int* piface, CanasCanFrame* pframe, unsigned int timeout_usec)
{
    if (piface == NULL || pframe == NULL)
        return -1;
    FifoEntry fe;
    const unsigned int timeout_ticks = timeout_usec / (1000000 / configTICK_RATE_HZ);
    if (xQueueReceive(_queue_rx, &fe, timeout_ticks))
    {
        *pframe = fe.frame;
        *piface = fe.iface;
        return 1;
    }
    return 0;    // timed out
}

#else

static int _tryReceive(int* piface, CanasCanFrame* pframe)
{
    FifoEntry fe;
    __disable_irq();
    const int retval = _fifoPop(&_fifo_rx, &fe);
    __enable_irq();
    if (retval > 0)
    {
        *pframe = fe.frame;
        *piface = fe.iface;
    }
    return retval;
}

#if CAN_CHIBIOS

int canReceive(int* piface, CanasCanFrame* pframe, unsigned int timeout_usec)
{
    if (piface == NULL || pframe == NULL)
        return -1;

    EventListener listener;
    chEvtRegisterMask(&_on_rx, &listener, ALL_EVENTS);

    int res = _tryReceive(piface, pframe);
    if (!res)
    {
        chEvtWaitAnyTimeout(ALL_EVENTS, US2ST(timeout_usec));
        res = _tryReceive(piface, pframe);
    }
    chEvtUnregister(&_on_rx, &listener);
    return res;  // 0 or 1
}

#else

int canReceive(int* piface, CanasCanFrame* pframe)
{
    if (piface == NULL || pframe == NULL)
        return -1;
    return _tryReceive(piface, pframe);
}

#endif // CAN_CHIBIOS
#endif // CAN_FREERTOS
