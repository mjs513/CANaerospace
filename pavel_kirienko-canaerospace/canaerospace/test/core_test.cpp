/*
 * Tests for the main functionality
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include "test.hpp"

static char static_str_buf[CANAS_DUMP_BUF_LEN];

TEST(CoreTest, Simple)
{
    CanasConfig cfg = canasMakeConfig();
    CanasInstance inst;

    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasInit(&inst, &cfg, NULL)); // Default configuration must fail

    inst = makeGenericInstance();

    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, IFACE_COUNT - 1, NULL, 1));

    CanasCanFrame frm = makeFrame(123, 0, 90, CANAS_DATATYPE_NODATA, 0, 1);                 // Space-filler
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, _canasUpdateWithTimestamp(&inst, IFACE_COUNT, &frm, 2)); // Wrong iface index
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, _canasUpdateWithTimestamp(&inst, -1, &frm, 3));
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, IFACE_COUNT, NULL, 2));                   // Iface index must be ignored
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, -1, NULL, 3));

    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(1, iface_filters[i].size());
        EXPECT_EQ(CANAS_CAN_FLAG_RTR, iface_filters[i][0].mask);
    }
}

TEST(CoreTest, DataValidation)
{
    CanasInstance inst = makeGenericInstance();
    CanasCanFrame frm;

    frm = makeFrame(123, 0, MY_NODE_ID, CANAS_DATATYPE_CHAR2, 0, 1, 'a');            // 2 bytes expected, 1 given
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));

    frm = makeFrame(123, 0, MY_NODE_ID, CANAS_DATATYPE_ACHAR, 0, 1, 'a', 'b', 'c');  // 1 byte expected, 3 given
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));

    frm = makeFrame(123, 0, MY_NODE_ID, CANAS_DATATYPE_RESVD_BEGIN_, 0, 1);          // wrong data type
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));

    frm = makeFrame(123, 0, MY_NODE_ID, CANAS_DATATYPE_UDEF_BEGIN_, 0, 1);           // UDEF must be valid
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));

    frm = makeFrame(123, 0, MY_NODE_ID, CANAS_DATATYPE_FLOAT, 0, 1, 0, 0, 0, 0);     // correct
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));
}

TEST(CoreTest, ParamIgnoring) // Feed one frame, make sure that it appears in hook callback and not in param callback
{
    CanasInstance inst = makeGenericInstance();
    CanasCanFrame frm = makeFrame(123, 0, MY_NODE_ID, CANAS_DATATYPE_ACHAR2, 0, 1, 'a', 'b');
    cbcnt_param = cbcnt_hook = 0;
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));
    EXPECT_EQ(0, cbcnt_param);
    EXPECT_EQ(1, cbcnt_hook);
    std::cout << "Caught by hook: " << canasDumpMessage(&cbargs_hook.message, static_str_buf) << std::endl;
}

TEST(CoreTest, ParamReception) // Feed one frame, make sure that it appears both in hook callback and param callback
{
    resetMemory();

    CanasInstance inst = makeGenericInstance();
    CanasCanFrame frm = makeFrame(123, 0, 90, CANAS_DATATYPE_ACHAR2, 0, 1, 'a', 'b');

    EXPECT_EQ(-CANAS_ERR_BAD_REDUND_CHAN, canasParamSubscribe(&inst, 123, 0, cbParam, NULL)); // Invalid redund chan
    EXPECT_EQ(0, canasParamSubscribe(&inst, 123, 8, cbParam, NULL));
    EXPECT_EQ(1, mem_chunks.size());                                       // One chunk for one subscription

    // Fire:
    cbcnt_param = cbcnt_hook = 0;
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));
    EXPECT_EQ(1, cbcnt_param);
    EXPECT_EQ(1, cbcnt_hook);

    // Check data:
    EXPECT_EQ(90, cbargs_param.message.node_id);
    EXPECT_EQ(0,  cbargs_param.message.service_code);
    EXPECT_EQ(1,  cbargs_param.message.message_code);
    EXPECT_EQ(2,  cbargs_param.message.data.length);             // Must be initialized to real data size
    EXPECT_EQ(CANAS_DATATYPE_ACHAR2, cbargs_param.message.data.type);
    EXPECT_EQ('a', cbargs_param.message.data.container.ACHAR2[0]);
    EXPECT_EQ('b', cbargs_param.message.data.container.ACHAR2[1]);

    // Add garbage subscriptions:
    EXPECT_EQ(0, canasParamSubscribe(&inst, 8, 1, NULL, NULL));
    EXPECT_EQ(0, canasParamSubscribe(&inst, 9, 1, NULL, NULL));
    EXPECT_EQ(0, canasParamSubscribe(&inst, 10, 1, NULL, NULL));

    // Check the parameter cache (powered by magic):
    CanasParamCallbackArgs param_cb_args;
    EXPECT_EQ(0, canasParamRead(&inst, 123, 2, &param_cb_args));
    EXPECT_EQ(0, param_cb_args.timestamp_usec);
    EXPECT_EQ(123, param_cb_args.message_id);
    EXPECT_EQ(2, param_cb_args.redund_channel_id);
    EXPECT_EQ(0, canasParamRead(&inst, 123, 0, &param_cb_args));
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasParamRead(&inst, 123, 0, NULL));
    EXPECT_EQ(-CANAS_ERR_BAD_REDUND_CHAN, canasParamRead(&inst, 123, 200, &param_cb_args));
    EXPECT_EQ(-CANAS_ERR_NO_SUCH_ENTRY, canasParamRead(&inst, 50, 200, &param_cb_args));
    EXPECT_EQ(0, memcmp(&param_cb_args.message, &cbargs_param.message, sizeof(cbargs_param.message)));
    EXPECT_EQ(1, param_cb_args.timestamp_usec);

    // More tests:
    EXPECT_EQ(-CANAS_ERR_NOT_ENOUGH_MEMORY, canasParamSubscribe(&inst, 50, 255, NULL, NULL));
    EXPECT_EQ(-CANAS_ERR_ENTRY_EXISTS, canasParamSubscribe(&inst, 123, 1, NULL, NULL));
    EXPECT_EQ(-CANAS_ERR_BAD_MESSAGE_ID, canasParamSubscribe(&inst, CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN, 1, NULL, NULL));
    EXPECT_EQ(-CANAS_ERR_NO_SUCH_ENTRY, canasParamUnsubscribe(&inst, 50));

    // Unsubscribe and check that memory was released correctly:
    EXPECT_EQ(0, canasParamUnsubscribe(&inst, 9));
    EXPECT_EQ(0, canasParamUnsubscribe(&inst, 10));
    EXPECT_EQ(0, canasParamUnsubscribe(&inst, 8));
    EXPECT_EQ(0, canasParamUnsubscribe(&inst, 123));
    EXPECT_EQ(-CANAS_ERR_NO_SUCH_ENTRY, canasParamUnsubscribe(&inst, 8));
    EXPECT_EQ(-CANAS_ERR_NO_SUCH_ENTRY, canasParamUnsubscribe(&inst, 123));
    EXPECT_EQ(0, mem_chunks.size());
}

TEST(CoreTest, RepeatedParamFiltering)
{
    CanasInstance inst = makeGenericInstance();
    CanasCanFrame frm = makeFrame(123, 0, 90, CANAS_DATATYPE_ACHAR2, 0, 1, 'a', 'b');

    EXPECT_EQ(0, canasParamSubscribe(&inst, 123, 8, cbParam, NULL));
    cbcnt_param = cbcnt_hook = 0;
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));
    EXPECT_EQ(1, cbcnt_param);
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 1, &frm, 1)); // Repeat on another iface. This message must be rejected.
    EXPECT_EQ(1, cbcnt_param);
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 1, &frm, 60 * 1000 * 1000));// This must be passed because of timings (1 minute)
    EXPECT_EQ(2, cbcnt_param);
    EXPECT_EQ(3, cbcnt_hook);                                   // Hook must reflect every message, including repeated.
}

TEST(CoreTest, ParamPublication) // Publish a message and check that correct frame has been emitted
{
    CanasInstance inst = makeGenericInstance();
    // Advertise this parameter:
    EXPECT_EQ(0, canasParamAdvertise(&inst, 123, false));
    EXPECT_EQ(-CANAS_ERR_ENTRY_EXISTS, canasParamAdvertise(&inst, 123, true));
    EXPECT_EQ(-CANAS_ERR_BAD_MESSAGE_ID, canasParamAdvertise(&inst, CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN, false));

    // Perform publication:
    CanasMessageData data;
    memset(&data, 0, sizeof(data));
    data.container.ACHAR2[0] = 'a';
    data.container.ACHAR2[1] = 'b';
    data.type = CANAS_DATATYPE_ACHAR2;

    std::fill(iface_send_counter, iface_send_counter + IFACE_COUNT, 0);
    std::fill(iface_send_return_values, iface_send_return_values + IFACE_COUNT, 0);
    EXPECT_EQ(-CANAS_ERR_DRIVER, canasParamPublish(&inst, 123, &data, 34));// fn_send() will return 0, this is not okay

    std::fill(iface_send_counter, iface_send_counter + IFACE_COUNT, 0);
    std::fill(iface_send_return_values, iface_send_return_values + IFACE_COUNT, 1);
    iface_send_return_values[0] = 0;
    EXPECT_EQ(0, canasParamPublish(&inst, 123, &data, 34));                // One interface will fail, but this is okay
    EXPECT_EQ(-CANAS_ERR_NO_SUCH_ENTRY, canasParamPublish(&inst, 124, &data, 34));

    // At this point message_code will be 1, so we're expecting it:
    CanasCanFrame reference = makeFrame(123, 0, inst.config.node_id, CANAS_DATATYPE_ACHAR2, 34, 1, 'a', 'b');
    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(1, iface_send_counter[i]);
        EXPECT_EQ(0, memcmp(iface_send_dump + i, &reference, sizeof(reference)));
    }
}

TEST(CoreTest, ParamPublicationWithInterlacing)
{
    CanasInstance inst = makeGenericInstance();
    // Advertise this parameter:
    EXPECT_EQ(0, canasParamAdvertise(&inst, 123, true));

    // Perform publication:
    CanasMessageData data;
    memset(&data, 0, sizeof(data));
    data.container.ACHAR2[0] = 'a';
    data.container.ACHAR2[1] = 'b';
    data.type = CANAS_DATATYPE_ACHAR2;

    std::fill(iface_send_counter, iface_send_counter + IFACE_COUNT, 0);
    std::fill(iface_send_return_values, iface_send_return_values + IFACE_COUNT, 1);
    // Publish three times, each message goes into distinct interface
    EXPECT_EQ(0, canasParamPublish(&inst, 123, &data, 34));
    EXPECT_EQ(0, canasParamPublish(&inst, 123, &data, 34));
    EXPECT_EQ(0, canasParamPublish(&inst, 123, &data, 34));

    FOR_EACH_IFACE(i)
    {
        // Make sure that each interface was used only once
        EXPECT_EQ(1, iface_send_counter[i]);
        // Iface 0 will store message_code 0, iface 1 --> msgcode 1 and so on
        CanasCanFrame reference = makeFrame(123, 0, inst.config.node_id, CANAS_DATATYPE_ACHAR2, 34, i, 'a', 'b');
        EXPECT_EQ(0, memcmp(iface_send_dump + i, &reference, sizeof(reference)));
    }
}

TEST(CoreTest, ServiceIgnoring)
{
    resetMemory();

    CanasInstance inst = makeGenericInstance();
    cbcnt_srv_poll = cbcnt_srv_request = cbcnt_srv_response = cbcnt_hook = 0;

    // Service Request ID 34, redundancy channel must be ignored:
    CanasCanFrame frm = makeFrame(196, 2, MY_NODE_ID, CANAS_DATATYPE_ULONG, 8, 1, 0xde, 0xad, 0xfa, 0xce);
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 1, &frm, 1));
    frm = makeFrame(196, 2, MY_NODE_ID, CANAS_DATATYPE_RESVD_BEGIN_, 8, 1);
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, _canasUpdateWithTimestamp(&inst, 1, &frm, 1));
    EXPECT_EQ(0, cbcnt_srv_poll);
    EXPECT_EQ(0, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);
    EXPECT_EQ(1, cbcnt_hook);
    EXPECT_EQ(0xdeadface, cbargs_hook.message.data.container.ULONG);
    std::cout << "Caught by hook: " << canasDumpMessage(&cbargs_hook.message, static_str_buf) << std::endl;

    // Some subscriptions:
    EXPECT_EQ(0, canasServiceRegister(&inst, 8, cbSrvPoll, cbSrvRequest, cbSrvResponse, &inst));
    EXPECT_EQ(0, canasServiceRegister(&inst, 9, cbSrvPoll, cbSrvRequest, cbSrvResponse, &inst));
    EXPECT_EQ(0, canasServiceRegister(&inst, 10, NULL, NULL, NULL, &inst));
    EXPECT_EQ(-CANAS_ERR_ENTRY_EXISTS, canasServiceRegister(&inst, 8, NULL, NULL, NULL, &inst));
    EXPECT_EQ(-CANAS_ERR_NO_SUCH_ENTRY, canasServiceUnregister(&inst, 11));

    // This frame must be ignored despite the existing subscription, because it is wrongly addressed:
    frm = makeFrame(196, 2, 1, CANAS_DATATYPE_UDEF_BEGIN_, 8, 1, 0xca, 0xfe);
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 2, &frm, 2));
    EXPECT_EQ(0, cbcnt_srv_poll);
    EXPECT_EQ(0, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);
    EXPECT_EQ(2, cbcnt_hook);

    // This frame is correctly addressed but it has no corresponding subscription:
    frm = makeFrame(2000, 2, MY_NODE_ID, CANAS_DATATYPE_UDEF_BEGIN_, 11, 1, 0xca, 0xfe);
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 2, &frm, 2));
    EXPECT_EQ(0, cbcnt_srv_poll);
    EXPECT_EQ(0, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);
    EXPECT_EQ(3, cbcnt_hook);

    // Check if memory released correctly:
    EXPECT_EQ(0, canasServiceUnregister(&inst, 9));
    EXPECT_EQ(0, canasServiceUnregister(&inst, 8));
    EXPECT_EQ(0, canasServiceUnregister(&inst, 10));
    EXPECT_EQ(0, mem_chunks.size());
}

TEST(CoreTest, ServicePoll)
{
    CanasInstance inst = makeGenericInstance();
    cbcnt_srv_poll = cbcnt_srv_request = cbcnt_srv_response = cbcnt_hook = 0;

    // Fill subscriptions, 2 of them will get callbacks:
    EXPECT_EQ(0, canasServiceRegister(&inst, 8, cbSrvPoll, cbSrvRequest, cbSrvResponse, &inst));
    EXPECT_EQ(0, canasServiceRegister(&inst, 9, cbSrvPoll, cbSrvRequest, cbSrvResponse, &inst));
    EXPECT_EQ(0, canasServiceRegister(&inst, 10, NULL, NULL, NULL, &inst));

    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, NULL, 1));
    EXPECT_EQ(0, cbcnt_srv_poll);
    EXPECT_EQ(0, cbcnt_hook);

    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, -1, NULL, 1000000));
    EXPECT_EQ(2, cbcnt_srv_poll);
    EXPECT_EQ(0, cbcnt_hook);

    CanasCanFrame frm = makeFrame(196, 2, MY_NODE_ID, CANAS_DATATYPE_NODATA, 48, 1);
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, _canasUpdateWithTimestamp(&inst, -1, &frm, 2000000));
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, _canasUpdateWithTimestamp(&inst, IFACE_COUNT, &frm, 2000000));
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 2, &frm, 2000000));
    EXPECT_EQ(4, cbcnt_srv_poll);
    EXPECT_EQ(1, cbcnt_hook);

    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, -1, NULL, 2000001));
    EXPECT_EQ(4, cbcnt_srv_poll);
    EXPECT_EQ(1, cbcnt_hook);

    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, IFACE_COUNT, NULL, 3000000));
    EXPECT_EQ(6, cbcnt_srv_poll);
    EXPECT_EQ(1, cbcnt_hook);

    // These should remain unchanged:
    EXPECT_EQ(0, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);
}

TEST(CoreTest, ServiceReception)
{
    CanasInstance inst = makeGenericInstance();
    cbcnt_srv_request = cbcnt_srv_response = 0;

    EXPECT_EQ(0, canasServiceRegister(&inst, 8, cbSrvPoll, cbSrvRequest, cbSrvResponse, &inst));

    // Service Channel 0 (our own), redundancy channel must be ignored:
    CanasCanFrame frm = makeFrame(128, 2, MY_NODE_ID, CANAS_DATATYPE_ULONG, 8, 1, 0xde, 0xad, 0xfa, 0xce);
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));
    EXPECT_EQ(1, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);

    // Check data:
    EXPECT_TRUE(cbargs_srv_request.pstate == &inst);
    EXPECT_TRUE(cbargs_srv_request.service_channel == 0); // Service Channel == 0
    EXPECT_TRUE(cbargs_srv_request.timestamp_usec == 1);
    EXPECT_TRUE(cbargs_srv_request.message.data.container.ULONG == 0xdeadface);
    EXPECT_TRUE(cbargs_srv_request.message.data.type == CANAS_DATATYPE_ULONG);
    EXPECT_TRUE(cbargs_srv_request.message.service_code == 8);
    EXPECT_TRUE(cbargs_srv_request.message.node_id == MY_NODE_ID);

    // Service Response ID 101, so Message ID should be 2003:
    inst.config.service_channel = 101;
    frm = makeFrame(2003, 56, MY_NODE_ID + 1, CANAS_DATATYPE_UDEF_BEGIN_, 8, 1);
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 2));
    EXPECT_EQ(1, cbcnt_srv_request);
    EXPECT_EQ(1, cbcnt_srv_response);
}

TEST(CoreTest, ServiceRepetitions)
{
    CanasInstance inst = makeGenericInstance();
    cbcnt_srv_poll = cbcnt_srv_request = cbcnt_srv_response = cbcnt_hook = 0;

    EXPECT_EQ(0, canasServiceRegister(&inst, 8, cbSrvPoll, cbSrvRequest, cbSrvResponse, &inst));

    // Service Request ID 34:
    CanasCanFrame frm = makeFrame(196, 2, MY_NODE_ID, CANAS_DATATYPE_ULONG, 8, 1, 0xde, 0xad, 0xfa, 0xce);

    // This frame must be accepted, why not:
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 1));
    EXPECT_EQ(0, cbcnt_srv_poll);
    EXPECT_EQ(1, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);
    EXPECT_EQ(1, cbcnt_hook);

    // This frame is repeated on the same interface, so it must be accepted too:
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 10));
    EXPECT_EQ(0, cbcnt_srv_poll);
    EXPECT_EQ(2, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);
    EXPECT_EQ(2, cbcnt_hook);

    // Two previous frames repeating on another interfaces, and ALL of them must be discarded:
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 1, &frm, 20));
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 2, &frm, 30));
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 1, &frm, 40));
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 2, &frm, 50));
    EXPECT_EQ(0, cbcnt_srv_poll);
    EXPECT_EQ(2, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);
    EXPECT_EQ(6, cbcnt_hook);

    // Now this frame must be accepted ONCE because all interfaces have got equal number of repeated frames:
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 2, &frm, 60)); // Only this will pass the repetition filter (because it is first)
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 1, &frm, 70));
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 80));
    EXPECT_EQ(0, cbcnt_srv_poll);
    EXPECT_EQ(3, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);
    EXPECT_EQ(9, cbcnt_hook);

    // These 3 frames must pass because of timeouts:
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 0, &frm, 60 * 1000000));
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 1, &frm, 120 * 1000000));
    EXPECT_EQ(0, _canasUpdateWithTimestamp(&inst, 2, &frm, 180 * 1000000));
    EXPECT_EQ(3, cbcnt_srv_poll);                             // Meanwhile, we will get 3 polls due to timeouts
    EXPECT_EQ(6, cbcnt_srv_request);
    EXPECT_EQ(0, cbcnt_srv_response);
    EXPECT_EQ(12, cbcnt_hook);
}

TEST(CoreTest, ServiceState)
{
    CanasInstance inst = makeGenericInstance();

    EXPECT_EQ(0, canasServiceRegister(&inst, 8, cbSrvPoll, cbSrvRequest, NULL, &inst));
    EXPECT_EQ(0, canasServiceRegister(&inst, 9, cbSrvPoll, NULL, cbSrvResponse, &inst));
    EXPECT_EQ(0, canasServiceRegister(&inst, 10, NULL, NULL, NULL, &inst));

    void* pstate = NULL;
    EXPECT_EQ(0, canasServiceGetState(&inst, 8, &pstate));
    EXPECT_TRUE(pstate == &inst);

    pstate = NULL;
    EXPECT_EQ(0, canasServiceGetState(&inst, 9, &pstate));
    EXPECT_TRUE(pstate == &inst);

    EXPECT_EQ(0, canasServiceSetState(&inst, 9, reinterpret_cast<void*>(12345)));
    EXPECT_EQ(0, canasServiceGetState(&inst, 9, &pstate));
    EXPECT_TRUE(pstate == reinterpret_cast<void*>(12345));

    EXPECT_EQ(0, canasServiceSetState(&inst, 8, NULL));
    EXPECT_EQ(0, canasServiceGetState(&inst, 8, &pstate));
    EXPECT_TRUE(pstate == NULL);

    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasServiceGetState(&inst, 88, NULL));
    EXPECT_EQ(-CANAS_ERR_NO_SUCH_ENTRY, canasServiceGetState(&inst, 88, &pstate));
    EXPECT_EQ(-CANAS_ERR_NO_SUCH_ENTRY, canasServiceSetState(&inst, 88, pstate));
}

TEST(CoreTest, ServiceSending)
{
    CanasInstance inst = makeGenericInstance();

    // Select non-default channel:
    inst.config.service_channel = 2; // 2 --> Message IDs are 132/133

    CanasMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.message_code = 123;
    msg.node_id = MY_NODE_ID + 1;  // Foreign Node ID
    msg.service_code = 8;
    msg.data.container.LONG = 0xdeface11;
    msg.data.type = CANAS_DATATYPE_ULONG;

    std::fill(iface_send_counter, iface_send_counter + IFACE_COUNT, 0);
    std::fill(iface_send_return_values, iface_send_return_values + IFACE_COUNT, 1);
    iface_send_return_values[0] = 0;                                         // One interface will fail but it's okay

    // Send request:
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasServiceSendRequest(&inst, NULL));
    EXPECT_EQ(0, canasServiceSendRequest(&inst, &msg));

    // Check the frame passed to the driver:
    CanasCanFrame reference = makeFrame(132, 0, msg.node_id, CANAS_DATATYPE_ULONG, 8, 123, 0xde, 0xfa, 0xce, 0x11);
    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(1, iface_send_counter[i]);
        EXPECT_EQ(0, memcmp(iface_send_dump + i, &reference, sizeof(reference)));
    }

    // Wrong Node ID:
    msg.node_id = MY_NODE_ID;
    EXPECT_EQ(-CANAS_ERR_BAD_NODE_ID, canasServiceSendRequest(&inst, &msg));

    // Send response:
    EXPECT_EQ(-CANAS_ERR_BAD_SERVICE_CHAN, canasServiceSendResponse(&inst, &msg, CANAS_SERVICE_CHANNEL_LOW_MAX + 1));
    msg.node_id = MY_NODE_ID + 1;  // Foreign Node ID will fail
    EXPECT_EQ(-CANAS_ERR_BAD_NODE_ID, canasServiceSendResponse(&inst, &msg, 2));
    msg.node_id = MY_NODE_ID;      // This will work
    EXPECT_EQ(0, canasServiceSendResponse(&inst, &msg, 2));

    // Check the frame passed to the driver:
    reference = makeFrame(133, 0, msg.node_id, CANAS_DATATYPE_ULONG, 8, 123, 0xde, 0xfa, 0xce, 0x11);
    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(2, iface_send_counter[i]);
        EXPECT_EQ(0, memcmp(iface_send_dump + i, &reference, sizeof(reference)));
    }
}
