/*
 * SocketCAN adapter for the CANaerospace library
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#ifndef CANAEROSPACE_SOCKETCAN_H_
#define CANAEROSPACE_SOCKETCAN_H_

#include <canaerospace/driver.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Number of filters available for each interface.
 * In fact, there is no reasonable limit because filters are emulated by software inside the kernel.
 */
static const uint8_t CAN_FILTERS_PER_IFACE = 255;

/**
 * Initialize the interfaces.
 * @param [in]  pifnames    Array of the interface names, like "can0"
 * @param [out] out_sockets Array to be filled by opened file descriptors. Interface order will be preserved.
 * @param [in]  ifcount     Number of the interfaces, i.e. length of the both arrays.
 * @return 0 on success, negative on failure.
 */
int canInit(const char* pifnames[], int* out_sockets, int ifcount);

/**
 * Send a CAN frame through the socket.
 * You need to match the interface index with the corresponding socket descriptor.
 * @param [in] fd     Socket descriptor.
 * @param [in] pframe Pointer to the frame to be sent.
 * @return 1 if frame was sent, 0 if not, negative on failure.
 */
int canSend(int fd, const CanasCanFrame* pframe);

/**
 * Setup CAN filters for the specified socket.
 * You need to match the interface index with the corresponding socket descriptor.
 * @return 0 on success, negative on error.
 */
int canFilterSetup(int fd, const CanasCanFilterConfig* pfilters, int filters_len);

/**
 * Read a single frame from the socket.
 * You need to match the socket descriptor with the corresponding interface index.
 * @param [in] fd     Socket descriptor.
 * @param [in] pframe Pointer where to store the frame.
 * @return 1 if frame was received, 0 if not, negative on failure.
 */
int canReceive(int fd, CanasCanFrame* pframe);

#ifdef __cplusplus
}
#endif
#endif
