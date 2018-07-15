/*
 * Linux example for CANaerospace library
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include "canaerospace_linux.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <canaerospace/param_id/nod_default.h>
#include <canaerospace/services/std_identification.h>

static const int SPIN_TIMEOUT_MS = 10;
static const int PUB_INTERVAL_MS = 5000;
static const int IDS_QUERY_INTERVAL_MS = 30000;

static const int MY_NODE_ID      = 1;
static const int MY_REDUND_CHAN  = 0;
static const int MY_SERVICE_CHAN = 0;

static const int MY_HARDWARE_REVISION = 0x11;
static const int MY_SOFTWARE_REVISION = 0x22;

/**
 * Returns number of microseconds since UNIX epoch
 */
static uint64_t _timestampMicros(void)
{
    struct timeval tv;
    assert(gettimeofday(&tv, NULL) == 0);
    return ((uint64_t)tv.tv_sec) * 1000000ul + tv.tv_usec;
}

/**
 * This callback will be called when new parameter message arrives.
 * Note that each parameter may have a dedicated callback as well as share it with other parameters.
 */
static void _cbParamFloat(CanasInstance* pi, CanasParamCallbackArgs* pargs)
{
    assert(pi);
    assert(pargs);
    assert(pargs->message.data.type == CANAS_DATATYPE_FLOAT);
    CanasLinux* pcl = (CanasLinux*)pi->pthis;
    printf("PARAM %i[%i]   %s   float: %f\n", pargs->message_id, pargs->redund_channel_id,
        canasDumpMessage(&pargs->message, pcl->dump_buf), pargs->message.data.container.FLOAT);
}

/**
 * Publish/Read params
 */
static void _pollParams(CanasInstance* pi)
{
    time_t rawtime = time(NULL);
    struct tm* ptm = gmtime(&rawtime);
    /*
     * This parameter was advertised before.
     * Nothing special, just call canasParamPublish()
     */
    CanasMessageData msgd;
    msgd.type = CANAS_DATATYPE_CHAR4;   // This is standard type for this parameter
    msgd.container.CHAR4[0] = ptm->tm_hour;
    msgd.container.CHAR4[1] = ptm->tm_min;
    msgd.container.CHAR4[2] = ptm->tm_sec;
    msgd.container.CHAR4[3] = 0;
    assert(0 == canasParamPublish(pi, CANAS_NOD_DEF_UTC, &msgd, 0));
    /*
     * This parameter is advertised on-demand.
     * Note that in this case libcanaerospace is unable to run a dedicated counter for Message Code of this parameter,
     * so every publication will send a message with Message Code = 0. In most cases this behavior is undesirable.
     */
    msgd.type = CANAS_DATATYPE_CHAR4;   // This is standard type for this parameter
    msgd.container.CHAR4[0] = ptm->tm_mday;
    msgd.container.CHAR4[1] = ptm->tm_mon + 1;
    msgd.container.CHAR4[2] = (ptm->tm_year + 1900) / 100;
    msgd.container.CHAR4[3] = (ptm->tm_year + 1900) % 100;
    assert(0 == canasParamAdvertise(pi, CANAS_NOD_DEF_DATE, false));
    assert(0 == canasParamPublish(pi, CANAS_NOD_DEF_DATE, &msgd, 0));
    assert(0 == canasParamUnadvertise(pi, CANAS_NOD_DEF_DATE));

    // Collect the values of the subscribed parameter without callbacks:
    int redund_chan = 0;
    while (1)
    {
        CanasParamCallbackArgs cbargs;
        int res = canasParamRead(pi, CANAS_NOD_DEF_DC_SYSTEM_1_VOLTAGE, redund_chan, &cbargs);
        if (res == -CANAS_ERR_BAD_REDUND_CHAN)
            break;
        assert(res == 0);
        if (cbargs.timestamp_usec == 0)
            printf("Parameter %i[%i] was not received yet\n", CANAS_NOD_DEF_DC_SYSTEM_1_VOLTAGE, (int)redund_chan);
        else
            printf("Parameter %i[%i] has value %f updated %lu usec ago\n", CANAS_NOD_DEF_DC_SYSTEM_1_VOLTAGE,
                (int)redund_chan, cbargs.message.data.container.FLOAT,
                (_timestampMicros() - cbargs.timestamp_usec) / 1000u);
        redund_chan++;
    }
}

static void _idsCallback(CanasInstance* pi, uint8_t node_id, CanasSrvIdsPayload* ppayload)
{
    assert(pi);
    assert(node_id > 0);
    if (ppayload == NULL) // This request was timed out, so there is no such node in the network
        return;
    printf("IDS response from node 0x%02x: HW 0x%02x, SW 0x%02x, ID-distr 0x%02x, Header 0x%02x\n",
           (int)node_id, (int)ppayload->hardware_revision, (int)ppayload->software_revision,
           (int)ppayload->id_distribution, (int)ppayload->header_type);
}

/**
 * Scan the network to find new nodes.
 * Read the specification to learn more about IDS service.
 */
static void _performIdsQuery(CanasInstance* pi)
{
    puts("Broadcasting the IDS request...");
    assert(0 == canasSrvIdsRequest(pi, CANAS_BROADCAST_NODE_ID, _idsCallback));
}

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <first_iface> [second_iface ...]\n", argv[0]);
        return 1;
    }

    // Initialize the CANaerospace instance:
    CanasInstance inst;
    int res = canasLinuxInit(&inst, argv + 1, argc - 1, MY_NODE_ID, MY_REDUND_CHAN, MY_SERVICE_CHAN);
    assert(res == 0);
    puts("Initialized successfully");

    // Initialize IDS service. CANaerospace specification requires it, though libcanaerospace does not:
    CanasSrvIdsPayload ids_selfdescr;
    ids_selfdescr.hardware_revision = MY_HARDWARE_REVISION;
    ids_selfdescr.software_revision = MY_SOFTWARE_REVISION;
    ids_selfdescr.id_distribution   = CANAS_SRV_IDS_ID_DISTRIBUTION_STD;
    ids_selfdescr.header_type       = CANAS_SRV_IDS_HEADER_TYPE_STD;
    assert(0 == canasSrvIdsInit(&inst, &ids_selfdescr, CANAS_SRV_IDS_MAX_PENDING_REQUESTS));

    // Create subscriptions and advertisements:
    assert(0 == canasParamAdvertise(&inst, CANAS_NOD_DEF_UTC, false));
    assert(0 == canasParamSubscribe(&inst, CANAS_NOD_DEF_OUTSIDE_AIR_TEMPERATURE, 3, _cbParamFloat, NULL));
    assert(0 == canasParamSubscribe(&inst, CANAS_NOD_DEF_DC_SYSTEM_1_VOLTAGE, 3, NULL, NULL));

    uint64_t last_param_polling = _timestampMicros();
    uint64_t last_ids_query = _timestampMicros();
    for (;;)
    {
        res = canasLinuxSpinOnce(&inst, SPIN_TIMEOUT_MS);
        assert(res == 0);

        if (_timestampMicros() - last_param_polling > (unsigned int)(PUB_INTERVAL_MS * 1000))
        {
            last_param_polling = _timestampMicros();
            _pollParams(&inst);
        }

        if (_timestampMicros() - last_ids_query > (unsigned int)(IDS_QUERY_INTERVAL_MS * 1000))
        {
            last_ids_query = _timestampMicros();
            _performIdsQuery(&inst);
        }
    }
    return 0;
}
