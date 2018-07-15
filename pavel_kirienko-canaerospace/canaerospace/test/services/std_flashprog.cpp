/*
 * Tests for FPS service
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include "../test.hpp"
#include <canaerospace/services/std_flashprog.h>

namespace
{
    uint8_t security_code = 123;

    int incoming_request_count = 0;
    uint8_t incoming_request_security_code = 0;
    int8_t response_for_next_request = 0;

    int8_t cbRequest(CanasInstance* pi, uint8_t security_code)
    {
        CHECKPTR(pi);
        incoming_request_count++;
        incoming_request_security_code = security_code;
        return response_for_next_request;
    }

    int response_count = 0;

    struct LastResponse
    {
        uint8_t node_id;
        bool timed_out;
        int8_t response_code;
        void* parg;
    } last_response;

    void cbResponse(CanasInstance* pi, uint8_t node_id, bool timed_out, int8_t signed_response_code, void* parg)
    {
        CHECKPTR(pi);
        response_count++;
        last_response.node_id = node_id;
        last_response.timed_out = timed_out;
        last_response.response_code = signed_response_code;
        last_response.parg = parg;
    }
}

TEST(StdSrvFPS, IncomingRequest)
{
    CanasInstance inst = makeGenericInstance();

    EXPECT_EQ(0, canasSrvFpsInit(&inst, cbRequest));

    FOR_EACH_IFACE(i)
    {
        iface_send_return_values[i] = 1;
        iface_send_counter[i] = 0;
    }
    incoming_request_count = 0;
    incoming_request_security_code = 0;
    response_for_next_request = CANAS_SRV_FPS_RESULT_OK;

    // Simple request with valid result:
    CanasCanFrame frm = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MAX - 1, 0,
        inst.config.node_id, CANAS_DATATYPE_NODATA, 6, security_code);
    EXPECT_EQ(0, canasUpdate(&inst, 0, &frm));

    EXPECT_EQ(1, incoming_request_count);
    EXPECT_EQ(security_code, incoming_request_security_code);

    CanasCanFrame frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MAX, 0,
        inst.config.node_id, CANAS_DATATYPE_NODATA, 6, CANAS_SRV_FPS_RESULT_OK);
    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(1, iface_send_counter[i]);
        EXPECT_TRUE(0 == std::memcmp(&iface_send_dump[i], &frm_response, sizeof(frm_response)));
    }

    // Return error with broadcast request:
    response_for_next_request = CANAS_SRV_FPS_RESULT_INVALID_SECURITY_CODE;

    frm = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MAX - 1, 0,
        CANAS_BROADCAST_NODE_ID, CANAS_DATATYPE_NODATA, 6, security_code);
    EXPECT_EQ(0, canasUpdate(&inst, 0, &frm));

    EXPECT_EQ(2, incoming_request_count);
    EXPECT_EQ(security_code, incoming_request_security_code);

    frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MAX, 0,
        inst.config.node_id, CANAS_DATATYPE_NODATA, 6, CANAS_SRV_FPS_RESULT_INVALID_SECURITY_CODE);

    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(2, iface_send_counter[i]);
        EXPECT_TRUE(0 == std::memcmp(&iface_send_dump[i], &frm_response, sizeof(frm_response)));
    }
}

TEST(StdSrvFPS, IncomingRequestWithNoHandler)
{
    CanasInstance inst = makeGenericInstance();

    EXPECT_EQ(0, canasSrvFpsInit(&inst, NULL));  // No callback.
    FOR_EACH_IFACE(i)
    {
        iface_send_return_values[i] = 1;
        iface_send_counter[i] = 0;
    }
    incoming_request_count = 0;
    incoming_request_security_code = 0;
    response_for_next_request = CANAS_SRV_FPS_RESULT_OK;

    // Send request, it must produce an error:
    CanasCanFrame frm = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MAX - 1, 0,
        inst.config.node_id, CANAS_DATATYPE_NODATA, 6, security_code);
    EXPECT_EQ(0, canasUpdate(&inst, 0, &frm));

    EXPECT_EQ(0, incoming_request_count);

    CanasCanFrame frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MAX, 0,
        inst.config.node_id, CANAS_DATATYPE_NODATA, 6, CANAS_SRV_FPS_RESULT_ABORT);
    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(1, iface_send_counter[i]);
        EXPECT_TRUE(0 == std::memcmp(&iface_send_dump[i], &frm_response, sizeof(frm_response)));
    }
}

TEST(StdSrvFPS, OutgoingRequest)
{
    CanasInstance inst = makeGenericInstance();

    uint8_t target_node = inst.config.node_id + 1;
    CanasCanFrame frm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0,
        target_node, CANAS_DATATYPE_NODATA, 6, CANAS_SRV_FPS_RESULT_INVALID_SECURITY_CODE);

    void* cb_arg = &inst;
    EXPECT_EQ(0, canasSrvFpsInit(&inst, cbRequest));

    // Send request:
    EXPECT_EQ(-CANAS_ERR_BAD_NODE_ID,        // Broadcast requests are not allowed
              canasSrvFpsRequest(&inst, CANAS_BROADCAST_NODE_ID, security_code, cbResponse, cb_arg));
    EXPECT_EQ(0, canasSrvFpsRequest(&inst, target_node, security_code, cbResponse, cb_arg));
    EXPECT_EQ(-CANAS_ERR_QUOTA_EXCEEDED,     // No concurrent requests allowed
              canasSrvFpsRequest(&inst, target_node, security_code, cbResponse, cb_arg));

    // Emulate response:
    response_count = 0;
    EXPECT_EQ(0, canasUpdate(&inst, 0, &frm_response));
    EXPECT_EQ(1, response_count);

    // Check response data:
    EXPECT_EQ(target_node, last_response.node_id);
    EXPECT_FALSE(last_response.timed_out);
    EXPECT_EQ(CANAS_SRV_FPS_RESULT_INVALID_SECURITY_CODE, last_response.response_code);
    EXPECT_TRUE(cb_arg == last_response.parg);

    // Timeout:
    EXPECT_EQ(0, canasSrvFpsRequest(&inst, target_node, security_code, cbResponse, cb_arg));
    current_timestamp += 10000000;
    EXPECT_EQ(0, canasUpdate(&inst, -1, NULL));

    // Check callback data:
    EXPECT_EQ(target_node, last_response.node_id);
    EXPECT_TRUE(last_response.timed_out);
    EXPECT_TRUE(cb_arg == last_response.parg);
}
