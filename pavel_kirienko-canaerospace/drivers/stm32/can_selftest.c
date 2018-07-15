/*
 * Self testing feature
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <stm32f10x_can.h>

#define SLAVE_START_BANK 14

/**
 * Stolen shamelessly from STM32 SPL examples
 */
int canSelftest(CAN_TypeDef* CANx)
{
    CAN_InitTypeDef CAN_InitStructure;
    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    CanTxMsg TxMessage;
    CanRxMsg RxMessage;
    volatile uint32_t i = 0;
    uint8_t TransmitMailbox = 0;
    /* CAN cell init */
    CAN_DeInit(CANx);
    CAN_StructInit(&CAN_InitStructure);
    CAN_InitStructure.CAN_TTCM = DISABLE;
    CAN_InitStructure.CAN_ABOM = DISABLE;
    CAN_InitStructure.CAN_AWUM = DISABLE;
    CAN_InitStructure.CAN_NART = DISABLE;
    CAN_InitStructure.CAN_RFLM = DISABLE;
    CAN_InitStructure.CAN_TXFP = DISABLE;
    CAN_InitStructure.CAN_Mode = CAN_Mode_Silent_LoopBack;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStructure.CAN_BS1 = CAN_BS1_6tq;
    CAN_InitStructure.CAN_BS2 = CAN_BS2_5tq;
    CAN_InitStructure.CAN_Prescaler = 3;
    if (CAN_Init(CANx, &CAN_InitStructure) != CAN_InitStatus_Success)
        return -1;
    /* CAN filter init */
    CAN_SlaveStartBank(SLAVE_START_BANK);
    if (CANx == CAN1)
        CAN_FilterInitStructure.CAN_FilterNumber = 0;
    else if (CANx == CAN2)
        CAN_FilterInitStructure.CAN_FilterNumber = SLAVE_START_BANK;
    else
        return -1;
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = 0;
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&CAN_FilterInitStructure);
    /* transmit */
    TxMessage.StdId = 0x11;
    TxMessage.RTR = CAN_RTR_DATA;
    TxMessage.IDE = CAN_ID_STD;
    TxMessage.DLC = 2;
    TxMessage.Data[0] = 0xCA;
    TxMessage.Data[1] = 0xFE;
    TransmitMailbox = CAN_Transmit(CANx, &TxMessage);
    i = 0;
    while ((CAN_TransmitStatus(CANx, TransmitMailbox) != CANTXOK) && (i != 0xFFFF))
        i++;
    i = 0;
    while ((CAN_MessagePending(CANx, CAN_FIFO0) < 1) && (i != 0xFFFF))
        i++;
    /* receive */
    RxMessage.StdId = 0x00;
    RxMessage.IDE = CAN_ID_STD;
    RxMessage.DLC = 0;
    RxMessage.Data[0] = 0x00;
    RxMessage.Data[1] = 0x00;
    CAN_Receive(CANx, CAN_FIFO0, &RxMessage);
    if (RxMessage.StdId != 0x11)
        return -2;
    if (RxMessage.IDE != CAN_ID_STD)
        return -3;
    if (RxMessage.DLC != 2)
        return -4;
    if ((RxMessage.Data[0] << 8 | RxMessage.Data[1]) != 0xCAFE)
        return -5;
    return 0;
}
