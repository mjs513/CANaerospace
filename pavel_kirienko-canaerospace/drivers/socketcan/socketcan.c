/*
 * SocketCAN adapter for the CANaerospace library
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "socketcan.h"

static int _sock(const char* ifname)
{
    const int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    struct ifreq ifr;   // If you get "storage size of ‘ifr’ isn’t known" here, disable strict ANSI mode: "-std=gnu99"
    strcpy(ifr.ifr_name, ifname);
    if (ioctl(s, SIOCGIFINDEX, &ifr) != 0)
        return -1;
    /*
     * If there is no interface with this name, neither ioctl() nor bind() will fail.
     * If this is the case, this function will return success but socket will fail at first write().
     */
    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) != 0)
        return -1;
    return s;
}

int canInit(const char* pifnames[], int* out_sockets, int ifcount)
{
    for (int i = 0; i < ifcount; i++)
        out_sockets[i] = -1;

    for (int i = 0; i < ifcount; i++)
    {
        int s = _sock(pifnames[i]);
        if (s < 0)
            goto error;
        out_sockets[i] = s;
    }
    return 0;

    error:
    for (int i = 0; i < ifcount; i++)
        close(out_sockets[i]);
    return -1;
}

int canSend(int fd, const CanasCanFrame* pframe)
{
    if (pframe == NULL)
        return -1;

    if (pframe->dlc > 8)
        return -1;     // wtf

    struct can_frame frame;
    memset(&frame, 0, sizeof(struct can_frame));

    frame.can_id = pframe->id & ((pframe->id & CANAS_CAN_FLAG_EFF) ? CANAS_CAN_MASK_EXTID : CANAS_CAN_MASK_STDID);
    if (pframe->id & CANAS_CAN_FLAG_EFF)
        frame.can_id |= CAN_EFF_FLAG;
    if (pframe->id & CANAS_CAN_FLAG_RTR)
        frame.can_id |= CAN_RTR_FLAG;

    memcpy(frame.data, pframe->data, pframe->dlc);
    frame.can_dlc = pframe->dlc;

    int written = write(fd, &frame, sizeof(struct can_frame));
    if (written <= 0)
        return written;
    if (written != sizeof(struct can_frame))
        return -1; // failed to write entire frame
    return 1;
}

int canReceive(int fd, CanasCanFrame* pframe)
{
    if (pframe == NULL)
        return -1;

    struct can_frame frame;
    memset(&frame, 0, sizeof(struct can_frame));
    memset(pframe, 0, sizeof(*pframe));

    int res = read(fd, &frame, sizeof(struct can_frame));
    if (res <= 0)
        return res;
    if (res != sizeof(struct can_frame))
        return -1; // epic fail, this frame is incomplete or we're using wrong socket

    pframe->dlc = frame.can_dlc;
    memcpy(pframe->data, frame.data, frame.can_dlc);

    pframe->id = frame.can_id & ((frame.can_id & CAN_EFF_FLAG) ? CAN_EFF_MASK : CAN_SFF_MASK);
    if (frame.can_id & CAN_EFF_FLAG)
        pframe->id |= CANAS_CAN_FLAG_EFF;
    if (frame.can_id & CAN_RTR_FLAG)
        pframe->id |= CANAS_CAN_FLAG_RTR;

    return 1;
}

int canFilterSetup(int fd, const CanasCanFilterConfig* pfilters, int filters_len)
{
    if (pfilters == NULL || filters_len <= 0)
        return -1;

    struct can_filter* pfkern = calloc(filters_len, sizeof(struct can_filter));
    if (pfkern == NULL)
        return -1;

    for (int i = 0; i < filters_len; i++)
    {
        pfkern[i].can_id   = pfilters[i].id   & CANAS_CAN_MASK_EXTID;
        pfkern[i].can_mask = pfilters[i].mask & CANAS_CAN_MASK_EXTID;

        if (pfilters[i].id & CANAS_CAN_FLAG_EFF)
            pfkern[i].can_id |= CAN_EFF_FLAG;

        if (pfilters[i].id & CANAS_CAN_FLAG_RTR)
            pfkern[i].can_id |= CAN_RTR_FLAG;

        if (pfilters[i].mask & CANAS_CAN_FLAG_EFF)
            pfkern[i].can_mask |= CAN_EFF_FLAG;

        if (pfilters[i].mask & CANAS_CAN_FLAG_RTR)
            pfkern[i].can_mask |= CAN_RTR_FLAG;
    }

    int ret = setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, pfkern, sizeof(struct can_filter) * filters_len);

    free(pfkern);
    return (ret < 0) ? -1 : 0;
}

