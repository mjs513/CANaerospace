/*
 * Standard Node Synchronization Service
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <endian.h>
#include <canaerospace/services/std_nodesync.h>
#include "../test.hpp"

namespace
{
    const int SERVICE_CODE = 1;

    volatile bool callback_received = false;
    uint32_t callback_timestamp = 0;

    void callback(CanasInstance* pi, uint32_t timestamp)
    {
        CHECKPTR(pi);
        callback_received = true;
        callback_timestamp = timestamp;
    }
}

TEST(StdSrvNSS, Everything)
{
    CanasInstance inst = makeGenericInstance();

    FOR_EACH_IFACE(i)
    {
        iface_send_return_values[i] = 1;
        iface_send_counter[i] = 0;
    }

    ASSERT_EQ(0, canasSrvNssInit(&inst, callback));
    ASSERT_NE(0, canasSrvNssInit(&inst, callback));

    // Send NSS request:
    const uint32_t timestamp = 123456;
    ASSERT_EQ(0, canasSrvNssPublish(&inst, timestamp));
    FOR_EACH_IFACE(i)
    {
        ASSERT_EQ(1, iface_send_counter[i]);
        CanasMessage msg = extractCanasMessage(iface_send_dump[i]);
        ASSERT_EQ(0, msg.message_code);
        ASSERT_EQ(CANAS_BROADCAST_NODE_ID, msg.node_id);
        ASSERT_EQ(SERVICE_CODE,            msg.service_code);
        ASSERT_EQ(CANAS_DATATYPE_ULONG,    msg.data.type);
        ASSERT_EQ(timestamp,       be32toh(msg.data.container.ULONG));
    }

    // Make incoming request:
    CanasCanFrame frame = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN, 0, CANAS_BROADCAST_NODE_ID,
        CANAS_DATATYPE_ULONG, SERVICE_CODE, 0,
        0xde, 0xad, 0xfa, 0xce);

    callback_received = false;
    ASSERT_EQ(0, canasUpdate(&inst, 0, &frame));

    ASSERT_TRUE(callback_received);
    ASSERT_EQ(0xdeadface, callback_timestamp);

    // Must not be changed:
    FOR_EACH_IFACE(i)
        ASSERT_EQ(1, iface_send_counter[i]);
}
