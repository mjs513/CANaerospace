/*
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <ch.h>
#include <hal.h>
#include <stdio.h>
#include <string.h>
#include "sys/sys.h"
#include <can_driver.h>
#include <canaerospace/canaerospace.h>
#include <canaerospace/param_id/nod_default.h>
#include <canaerospace/services/std_identification.h>

static const int MY_NODE_ID      = 73;
static const int MY_REDUND_CHAN  = 1;
static const int MY_SERVICE_CHAN = 8;

static const int MY_HARDWARE_REVISION = 0xde;
static const int MY_SOFTWARE_REVISION = 0xad;

static CanasInstance _canas_instance;
static MUTEX_DECL(_canas_mutex);

static WORKING_AREA(waReader, 2048);
static msg_t readerThread(void* arg)
{
    (void)arg;
    static const int UPDATE_INTERVAL_USEC = 10 * 1000;

    chRegSetThreadName("canrx");
    while (1)
    {
        CanasCanFrame frm;
        int iface = -1;
        int ret = canReceive(&iface, &frm, UPDATE_INTERVAL_USEC);
        if (ret < 0)
        {
            printf("CAN RX failed: %i\n", ret);
            continue;
        }
        chMtxLock(&_canas_mutex);
        {
            if (ret == 0)
                ret = canasUpdate(&_canas_instance, -1, NULL);     // No data received
            else
            {
                ASSERT(iface >= 0 && iface < CAN_IFACE_COUNT);
                ret = canasUpdate(&_canas_instance, iface, &frm);
            }
        }
        chMtxUnlock();
        if (ret != 0)
            printf("Temporary failure in canasUpdate(): %i\n", ret);
    }
    return 0;
}

static WORKING_AREA(waMonitor, 2048);
static msg_t monitorThread(void* arg)
{
    (void)arg;
    while (1)
    {
        chThdSleepSeconds(2);
        bool have_errors = false;
        for (int i = 0; i < CAN_IFACE_COUNT; i++)
        {
            chMtxLock(&_canas_mutex);
            const unsigned int errmask = canYieldErrors(i);
            chMtxUnlock();
            if (errmask)
            {
                have_errors = true;
                printf("CAN%i errmask %04x\n", i, errmask);
            }
        }
        if (have_errors)
            palSetPad(GPIOB, GPIOB_LED);
        else
            palClearPad(GPIOB, GPIOB_LED);
    }
    return 0;
}

static int drvSend(CanasInstance* pi, int iface, const CanasCanFrame* pframe)
{
    ASSERT(pi);
    ASSERT(iface >= 0 && iface < CAN_IFACE_COUNT);
    ASSERT(pframe);
    return canSend(iface, pframe);
}

static int drvFilter(CanasInstance* pi, int iface, const CanasCanFilterConfig* pfilters, int nfilters)
{
    ASSERT(pi);
    ASSERT(iface >= 0 && iface < CAN_IFACE_COUNT);
    ASSERT(pfilters);
    ASSERT(nfilters > 0);
    return canFilterSetup(iface, pfilters, nfilters);
}

static void* canasImplMalloc(CanasInstance* pi, int size)
{
    ASSERT(pi);
    return chCoreAlloc(size);
}

static uint64_t canasImplTimestamp(CanasInstance* pi)
{
    ASSERT(pi);
    return sysTimestampMicros();
}

static void init(void)
{
    ASSERT_ALWAYS(0 == canInit(1000000, 4000));

    CanasConfig cfg = canasMakeConfig();
    cfg.filters_per_iface = CAN_FILTERS_PER_IFACE;
    cfg.iface_count  = CAN_IFACE_COUNT;
    cfg.fn_send      = drvSend;
    cfg.fn_filter    = drvFilter;
    cfg.fn_malloc    = canasImplMalloc;
    cfg.fn_timestamp = canasImplTimestamp;
    // Each function pointer is initialized to NULL by default.
    // Application params:
    cfg.node_id           = MY_NODE_ID;
    cfg.redund_channel_id = MY_REDUND_CHAN;
    cfg.service_channel   = MY_SERVICE_CHAN;

    chMtxInit(&_canas_mutex);

    ASSERT_ALWAYS(0 == canasInit(&_canas_instance, &cfg, NULL));

    // IDS service (it is mandatory for every node):
    CanasSrvIdsPayload ids_selfdescr;
    ids_selfdescr.hardware_revision = MY_HARDWARE_REVISION;
    ids_selfdescr.software_revision = MY_SOFTWARE_REVISION;
    ids_selfdescr.id_distribution   = CANAS_SRV_IDS_ID_DISTRIBUTION_STD;  // These two are standard
    ids_selfdescr.header_type       = CANAS_SRV_IDS_HEADER_TYPE_STD;
    ASSERT_ALWAYS(0 == canasSrvIdsInit(&_canas_instance, &ids_selfdescr, CANAS_MAX_NODES));
}

static void idsCallback(CanasInstance* pi, uint8_t node_id, CanasSrvIdsPayload* ppayload)
{
    ASSERT(pi);
    ASSERT(node_id > 0);
    if (ppayload == NULL) // This request was timed out, so there is no such node in the network
        return;
    printf("IDS response from node 0x%02x: HW 0x%02x, SW 0x%02x, ID-distr 0x%02x, Header 0x%02x\n",
           (int)node_id, (int)ppayload->hardware_revision, (int)ppayload->software_revision,
           (int)ppayload->id_distribution, (int)ppayload->header_type);
}

int main(void)
{
    halInit();
    chSysInit();
    sdStart(&SD2, NULL);

    init();

    puts("Initialized");

    chThdCreateStatic(waReader, sizeof(waReader), HIGHPRIO, readerThread, NULL);
    chThdCreateStatic(waMonitor, sizeof(waMonitor), LOWPRIO, monitorThread, NULL);

    while (1)
    {
        chThdSleepSeconds(10);
        puts("IDS discovery...");
        ASSERT(0 == canasSrvIdsRequest(&_canas_instance, CANAS_BROADCAST_NODE_ID, idsCallback));
    }
    return 0;
}
