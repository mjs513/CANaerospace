/*
 * Tests for IDS service
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include "../test.hpp"
#include <canaerospace/services/std_identification.h>

namespace
{
    CanasSrvIdsPayload _makePayload()
    {
        CanasSrvIdsPayload payld;
        payld.hardware_revision = 1;
        payld.software_revision = 2;
        payld.id_distribution   = 3;
        payld.header_type       = 4;
        return payld;
    }

    int done_callback_calls = 0;
    uint8_t done_last_node_id = 0;
    bool done_timed_out = false;
    CanasSrvIdsPayload done_last_payload;

    void _doneCallback(CanasInstance*, uint8_t node_id, CanasSrvIdsPayload* ppayload)
    {
        done_callback_calls++;
        done_last_node_id = node_id;
        done_timed_out = ppayload == NULL;
        if (ppayload != NULL)
            done_last_payload = *ppayload;
        else
            std::memset(&done_last_payload, 0, sizeof(done_last_payload));
    }
}

TEST(StdSrvIDS, Response)
{
    CanasInstance inst = makeGenericInstance();
    CanasSrvIdsPayload payld = _makePayload();

    CanasCanFrame frm_request = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN, 0, inst.config.node_id,
        CANAS_DATATYPE_NODATA, 0, 0);

    CanasCanFrame frm_expected_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN + 1, 0, inst.config.node_id,
        CANAS_DATATYPE_UCHAR4, 0, 0,
        payld.hardware_revision, payld.software_revision, payld.id_distribution, payld.header_type);

    // Init service
    EXPECT_EQ(0, canasSrvIdsInit(&inst, &payld, 8));

    FOR_EACH_IFACE(i)
        iface_send_counter[i] = 0;

    // Throw in the incoming request
    EXPECT_EQ(0, canasUpdate(&inst, 0, &frm_request));

    // Check results
    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(1, iface_send_counter[i]);
        EXPECT_EQ(0, std::memcmp(iface_send_dump + i, &frm_expected_response, sizeof(CanasCanFrame)));
    }
}

TEST(StdSrvIDS, RequestUnicast)
{
    CanasInstance inst = makeGenericInstance();
    CanasSrvIdsPayload payld = _makePayload();

    uint8_t target_node_id = 1;

    CanasCanFrame frm_expected_request = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN, 0, target_node_id,
        CANAS_DATATYPE_NODATA, 0, 0);

    FOR_EACH_IFACE(i)
    {
        iface_send_counter[i] = 0;
        iface_send_return_values[i] = 1;
    }

    EXPECT_EQ(0, canasSrvIdsInit(&inst, &payld, 1));

    // Send request:
    EXPECT_EQ(0, canasSrvIdsRequest(&inst, target_node_id, _doneCallback));
    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(1, iface_send_counter[i]);
        EXPECT_EQ(0, std::memcmp(iface_send_dump + i, &frm_expected_request, sizeof(CanasCanFrame)));
    }

    // Make sure that the outgoing request will fail in case if there is no free response slots:
    EXPECT_EQ(-CANAS_ERR_QUOTA_EXCEEDED, canasSrvIdsRequest(&inst, target_node_id, _doneCallback));
    // Note that the request above may produce spurious sendings, or may not, it is fully implementation dependent.

    // Simulate response:
    CanasCanFrame frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, target_node_id,
        CANAS_DATATYPE_UCHAR4, 0, 0,
        payld.hardware_revision, payld.software_revision, payld.id_distribution, payld.header_type);

    done_callback_calls = 0;
    EXPECT_EQ(0, canasUpdate(&inst, 0, &frm_response));

    // Check is received data correct:
    EXPECT_EQ(1, done_callback_calls);
    EXPECT_EQ(target_node_id, done_last_node_id);
    EXPECT_TRUE(0 == std::memcmp(&done_last_payload, &payld, sizeof(payld)));
}

TEST(StdSrvIDS, RequestBroadcast)
{
    memory_chunk_size_limit = 16384;

    CanasInstance inst = makeGenericInstance();
    CanasSrvIdsPayload payld = _makePayload();
    EXPECT_EQ(0, canasSrvIdsInit(&inst, &payld, 253));

    CanasCanFrame frm_expected_request = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN, 0, CANAS_BROADCAST_NODE_ID,
        CANAS_DATATYPE_NODATA, 0, 0);

    // Must fail - not enough response slots:
    EXPECT_EQ(-CANAS_ERR_QUOTA_EXCEEDED, canasSrvIdsRequest(&inst, CANAS_BROADCAST_NODE_ID, _doneCallback));

    FOR_EACH_IFACE(i)
    {
        iface_send_counter[i] = 0;
        iface_send_return_values[i] = 1;
    }

    // Must work well:
    inst = makeGenericInstance();                      // Full reinit
    EXPECT_EQ(0, canasSrvIdsInit(&inst, &payld, 254));

    // Send request:
    EXPECT_EQ(0, canasSrvIdsRequest(&inst, CANAS_BROADCAST_NODE_ID, _doneCallback));
    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(1, iface_send_counter[i]);
        EXPECT_EQ(0, std::memcmp(iface_send_dump + i, &frm_expected_request, sizeof(CanasCanFrame)));
    }

    // Feed responses and check the returned data:
    done_callback_calls = 0;
    for (int node = 1; node <= 255; node++)
    {
        if (node == inst.config.node_id)
            continue;
        // Make each frame received on every interface, why not:
        FOR_EACH_IFACE(i)
        {
            current_timestamp += 100;
            CanasCanFrame frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, node,
                CANAS_DATATYPE_UCHAR4, 0, 0,
                payld.hardware_revision, payld.software_revision, payld.id_distribution, payld.header_type);
            EXPECT_EQ(0, canasUpdate(&inst, i, &frm_response));
            // Check the returned data:
            EXPECT_FALSE(done_timed_out);
            EXPECT_EQ(node, done_last_node_id);
            EXPECT_TRUE(0 == std::memcmp(&done_last_payload, &payld, sizeof(payld)));
        }
    }
    EXPECT_EQ(CANAS_MAX_NODES - 1, done_callback_calls);

    // Make some timeouts:
    EXPECT_EQ(0, canasSrvIdsRequest(&inst, CANAS_BROADCAST_NODE_ID, _doneCallback));
    done_callback_calls = 0;
    for (int node = 1; node <= 255; node++)
    {
        if (node == inst.config.node_id)
            continue;
        if (node != 12 && node != 23 && node != 123)       // Allow only these nodes, others will be timed out
            continue;
        current_timestamp += 100;
        CanasCanFrame frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, node,
            CANAS_DATATYPE_UCHAR4, 0, 0,
            payld.hardware_revision, payld.software_revision, payld.id_distribution, payld.header_type);
        EXPECT_EQ(0, canasUpdate(&inst, node % IFACE_COUNT, &frm_response));          // Select the interface randomly
    }
    EXPECT_EQ(3, done_callback_calls);

    // Now fire callbacks that was timed out:
    current_timestamp += 100000000;
    EXPECT_EQ(0, canasUpdate(&inst, -1, NULL));

    // Check that every callback was there:
    EXPECT_TRUE(done_timed_out);
    EXPECT_EQ(CANAS_MAX_NODES - 1, done_callback_calls);

    // Make sure that no more callbacks will be invoked:
    done_callback_calls = 0;
    for (int node = 1; node <= 255; node++)
    {
        current_timestamp += 100;
        CanasCanFrame frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, node,
            CANAS_DATATYPE_UCHAR4, 0, 0,
            payld.hardware_revision, payld.software_revision, payld.id_distribution, payld.header_type);
        EXPECT_EQ(0, canasUpdate(&inst, node % IFACE_COUNT, &frm_response));          // Select the interface randomly
    }
    EXPECT_EQ(0, done_callback_calls);
}

TEST(StdSrvIDS, RequestMixed)
{
    memory_chunk_size_limit = 16384;

    CanasInstance inst = makeGenericInstance();
    CanasSrvIdsPayload payld = _makePayload();
    const int foreign_node_id = inst.config.node_id + 1;

    EXPECT_EQ(0, canasSrvIdsInit(&inst, &payld, 255));

    // Make requests to use all slots:
    EXPECT_EQ(0, canasSrvIdsRequest(&inst, foreign_node_id, _doneCallback));
    EXPECT_EQ(0, canasSrvIdsRequest(&inst, CANAS_BROADCAST_NODE_ID, _doneCallback));

    // Emulate responses:
    done_callback_calls = 0;
    for (int node = 1; node <= 255; node++)
    {
        if (node == inst.config.node_id)
            continue;
        current_timestamp += 100;
        CanasCanFrame frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, node,
            CANAS_DATATYPE_UCHAR4, 0, 0,
            payld.hardware_revision, payld.software_revision, payld.id_distribution, payld.header_type);
        EXPECT_EQ(0, canasUpdate(&inst, node % IFACE_COUNT, &frm_response));          // Select the interface randomly
    }
    EXPECT_EQ(254, done_callback_calls);

    // The last response:
    CanasCanFrame frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, foreign_node_id,
        CANAS_DATATYPE_UCHAR4, 0, 0,
        payld.hardware_revision, payld.software_revision, payld.id_distribution, payld.header_type);
    current_timestamp += 100;
    EXPECT_EQ(0, canasUpdate(&inst, 0, &frm_response));
    current_timestamp += 100;
    EXPECT_EQ(0, canasUpdate(&inst, 0, &frm_response));
    current_timestamp += 100;
    EXPECT_EQ(0, canasUpdate(&inst, 0, &frm_response));
    EXPECT_EQ(255, done_callback_calls);
}
