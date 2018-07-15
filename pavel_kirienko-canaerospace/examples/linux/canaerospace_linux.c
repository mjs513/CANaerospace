/*
 * Linux example for CANaerospace library
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "canaerospace_linux.h"

/**
 * See examples for embedded platforms to know how to avoid dynamic memory allocations.
 */
static void* _cbMalloc(CanasInstance* pi, int size)
{
    assert(pi);
    assert(size > 0);
    return malloc(size);
}

/**
 * This callback will never be called if you aren't using dynamic reconfiguration,
 * like unsubscription or unadvertisement.
 * Thus, it's optional.
 */
static void _cbFree(CanasInstance* pi, void* ptr)
{
    assert(pi);
    free(ptr);
}

/**
 * Note that each driver function needs socket fd as first parameter instead of iface index.
 */
static int _drvSend(CanasInstance* pi, int iface, const CanasCanFrame* pframe)
{
    assert(pi);
    CanasLinux* pcl = (CanasLinux*)pi->pthis;
    assert(iface >= 0);
    assert(iface < pcl->npollfds);
    assert(pframe);
    return canSend(pcl->pollfds[iface].fd, pframe);
}

static int _drvFilter(CanasInstance* pi, int iface, const CanasCanFilterConfig* pfilters, int nfilters)
{
    assert(pi);
    CanasLinux* pcl = (CanasLinux*)pi->pthis;
    assert(iface >= 0);
    assert(iface < pcl->npollfds);
    assert(pfilters);
    assert(nfilters > 0);
    return canFilterSetup(pcl->pollfds[iface].fd, pfilters, nfilters);
}

/**
 * This function is not provided by SocketCAN driver because your application may need to
 * perform more complex IO multiplexing with other sockets.
 */
static int _receive(CanasInstance* pi, int* piface, CanasCanFrame* pframe, int timeout_ms)
{
    CanasLinux* pcl = (CanasLinux*)pi->pthis;
    const int ret = poll(pcl->pollfds, pcl->npollfds, timeout_ms);
    if (ret <= 0)
        return ret;
    *piface = -1;
    for (int i = 0; i < pcl->npollfds; i++)
    {
        if (pcl->pollfds[i].revents & POLLIN)
        {
            *piface = i;
            break;
        }
    }
    if (*piface < 0)
        return 0;
    return canReceive(pcl->pollfds[*piface].fd, pframe);
}

static uint64_t _timestampMicros(CanasInstance* pi)
{
    (void)pi;
    struct timeval tv;
    assert(gettimeofday(&tv, NULL) == 0);
    return ((uint64_t)tv.tv_sec) * 1000000ul + tv.tv_usec;
}

int canasLinuxInit(CanasInstance* pi, const char* pifaces[], int nifaces,
                   int node_id, int redund_chan_id, int service_chan)
{
    // Pre-initialized config with default values provided where possible:
    CanasConfig cfg = canasMakeConfig();

    // Constant settings:
    cfg.filters_per_iface = CAN_FILTERS_PER_IFACE;
    cfg.fn_malloc = _cbMalloc;
    cfg.fn_free   = _cbFree;      // Optional (see manual)
    cfg.fn_send   = _drvSend;
    cfg.fn_filter = _drvFilter;
    cfg.fn_hook   = NULL;        // Optional
    cfg.fn_timestamp = _timestampMicros;

    // These settings are provided by the application:
    cfg.iface_count       = nifaces;
    cfg.node_id           = node_id;
    cfg.redund_channel_id = redund_chan_id;
    cfg.service_channel   = service_chan;

    // Initialize the CAN driver:
    int res = -1;
    int* psockets = alloca(sizeof(int) * nifaces);
    if ((res = canInit(pifaces, psockets, nifaces)) != 0)
    {
        fprintf(stderr, "Failed to initialize the CAN interfaces [%i]\n", res);
        return -1;
    }

    // Initialize auxiliary structure with platform-specific data:
    const int clsize = sizeof(CanasLinux) + sizeof(struct pollfd) * nifaces;
    CanasLinux* pcl = malloc(clsize);
    if (pcl == NULL)
    {
        perror("Strictly no elephants");
        return -1;
    }
    memset(pcl, 0, clsize);
    pcl->npollfds = nifaces;
    for (int i = 0; i < nifaces; i++)
    {
        pcl->pollfds[i].fd = psockets[i];
        pcl->pollfds[i].events = POLLIN;
    }

    // Initialize the instance of CANaerospace:
    if ((res = canasInit(pi, &cfg, pcl)) != 0)
    {
        fprintf(stderr, "Failed to initialize libcanaerospace [%i]\n", res);
        return -1;
    }
    return 0;
}

int canasLinuxSpinOnce(CanasInstance* pi, int timeout_ms)
{
    CanasCanFrame frame;
    int iface = -1;

    // Read the next incoming frame from any of the available interfaces:
    int res = _receive(pi, &iface, &frame, timeout_ms);
    if (res < 0)
        return res;

    // In case of timeout we need to update lib's state by calling canasUpdate() with pframe=NULL
    if (res)
        res = canasUpdate(pi, iface, &frame);
    else
        res = canasUpdate(pi, -1, NULL);

    // Temporary failure is possible if malformed frame received
    if (res)
        printf("CANaerospace update error: %i\n", res);
    return 0;
}
