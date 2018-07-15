/*
 * Linux example for CANaerospace library
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#ifndef CANAEROSPACE_LINUX_H_
#define CANAEROSPACE_LINUX_H_

#include <poll.h>
#include <canaerospace/canaerospace.h>
#include <canaerospace_drivers/socketcan/socketcan.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This structure contains a platform-specific data.
 */
typedef struct
{
    void* pappdata;
    char dump_buf[CANAS_DUMP_BUF_LEN];
    int npollfds;
    struct pollfd pollfds[];
} CanasLinux;

/**
 * Creates a new instance.
 */
int canasLinuxInit(CanasInstance* pi, const char* pifaces[], int nifaces,
                   int node_id, int redund_chan_id, int service_chan);

/**
 * Interval between subsequent calls should not be higher than 10ms.
 */
int canasLinuxSpinOnce(CanasInstance* pi, int timeout_ms);

#ifdef __cplusplus
}
#endif
#endif
