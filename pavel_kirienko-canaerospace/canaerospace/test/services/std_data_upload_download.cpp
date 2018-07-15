/*
 * Tests for DDS and DUS
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <endian.h>
#include "../test.hpp"
#include <canaerospace/services/std_data_upload_download.h>

#define UPDATE_TIME_STEP          20000
#define PAYLOAD_BYTES_PER_MESSAGE 4

namespace
{
    const int SERVICE_CODE_DDS = 2;
    const int SERVICE_CODE_DUS = 3;

    uint8_t msgcountByDatalen(uint16_t dataln) { return ((dataln - 1) / PAYLOAD_BYTES_PER_MESSAGE) + 1; }
    uint16_t datalenByMsgcount(uint8_t msgcnt) { return ((msgcnt + 1) * PAYLOAD_BYTES_PER_MESSAGE) - 1; }

    uint32_t computeChecksum(const uint8_t* pbuf, uint_fast16_t len)
    {
        uint32_t chksum = 0;
        for (uint_fast16_t i = 0; i < len; i++)
            chksum += pbuf[i];
        return chksum;
    }

    int32_t slave_download_request_return_value = CANAS_SRV_DDS_RESPONSE_ABORT;
    volatile bool slave_download_requested = false;
    volatile bool slave_download_done = false;
    uint16_t slave_download_datalen = 0;
    uint32_t slave_download_memid = 0;
    uint8_t slave_download_buff[CANAS_SRV_DATA_MAX_PAYLOAD_LEN];

    int32_t slaveDownloadRequestCallback(CanasInstance*, uint32_t memid, uint16_t expected_datalen)
    {
        EXPECT_LE(expected_datalen, CANAS_SRV_DATA_MAX_PAYLOAD_LEN);
        slave_download_requested = true;
        slave_download_datalen   = expected_datalen;
        slave_download_memid     = memid;
        return slave_download_request_return_value;
    }

    void slaveDownloadDoneCallback(CanasInstance*, uint32_t memid, void* pdata, uint16_t datalen)
    {
        CHECKPTR(pdata);
        EXPECT_LE(datalen, CANAS_SRV_DATA_MAX_PAYLOAD_LEN);
        slave_download_done    = true;
        slave_download_datalen = datalen;
        slave_download_memid   = memid;
        std::memcpy(slave_download_buff, pdata, datalen);
    }

    volatile bool slave_upload_requested = false;
    int32_t slave_upload_request_return_value = CANAS_SRV_DUS_RESPONSE_ABORT;
    uint32_t slave_upload_memid = 0;
    uint16_t slave_upload_datalen = 0;
    const uint8_t* slave_upload_pointer;

    int32_t slaveUploadRequestCallback(CanasInstance*, uint32_t memid, uint16_t expected_datalen,
                                       void* pdatabuff, uint16_t* pprovided_datalen_out)
    {
        CHECKPTR(pdatabuff);
        CHECKPTR(pprovided_datalen_out);
        EXPECT_LE(expected_datalen, CANAS_SRV_DATA_MAX_PAYLOAD_LEN);
        EXPECT_LE(slave_upload_datalen, CANAS_SRV_DATA_MAX_PAYLOAD_LEN);
        std::memcpy(pdatabuff, slave_upload_pointer, slave_upload_datalen);
        *pprovided_datalen_out = slave_upload_datalen;
        slave_upload_memid = memid;
        slave_upload_requested = true;
        return slave_upload_request_return_value;
    }

    bool master_download_done = false;
    CanasSrvDdsMasterDoneCallbackArgs master_download_done_args;

    void masterDownloadDoneCallback(CanasInstance* pi, CanasSrvDdsMasterDoneCallbackArgs* pargs)
    {
        CHECKPTR(pi);
        CHECKPTR(pargs);
        master_download_done = true;
        master_download_done_args = *pargs;
    }

    volatile bool master_upload_done = false;
    CanasSrvDusMasterDoneCallbackArgs master_upload_done_args;
    uint8_t master_upload_buff[CANAS_SRV_DATA_MAX_PAYLOAD_LEN];

    void masterUploadDoneCallback(CanasInstance* pi, CanasSrvDusMasterDoneCallbackArgs* pargs)
    {
        CHECKPTR(pi);
        CHECKPTR(pargs);
        master_upload_done = true;
        master_upload_done_args = *pargs;
        std::memcpy(master_upload_buff, pargs->pdata, pargs->datalen);
    }
}

/*
 * TODO: Add tests for XON/XOFF flow control
 */
TEST(StdSrvDDS, Transmission)
{
    memory_chunk_size_limit = 4096;
    CanasInstance inst = makeGenericInstance();
    ASSERT_EQ(0, canasSrvDataInit(&inst, 3,
        slaveDownloadRequestCallback, slaveDownloadDoneCallback, slaveUploadRequestCallback));

    uint8_t remote_node_id = inst.config.node_id + 1;
    uint32_t memid = 0xdeadbeef;
    const std::string data = "'You promised me Mars colonies. Instead, I got Facebook.' - Buzz Aldrin";
    const uint32_t checksum = computeChecksum(reinterpret_cast<const uint8_t*>(data.c_str()), data.length());
    const int msgcount = msgcountByDatalen(data.length());

    FOR_EACH_IFACE(i)
    {
        iface_send_return_values[i] = 1;
        iface_send_counter[i] = 0;
    }

    // Starting the download. This will produce an SDRM request on wire.
    ASSERT_EQ(0, canasSrvDdsDownloadTo(&inst, remote_node_id, memid, data.c_str(), data.length(),
        masterDownloadDoneCallback, &inst));
    // Will fail:
    ASSERT_EQ(-CANAS_ERR_ENTRY_EXISTS, canasSrvDdsDownloadTo(&inst, remote_node_id, memid, data.c_str(),
        data.length(), masterDownloadDoneCallback, NULL));
    ASSERT_EQ(-CANAS_ERR_ARGUMENT, canasSrvDdsDownloadTo(&inst, remote_node_id + 2, memid, data.c_str(),
        CANAS_SRV_DATA_MAX_PAYLOAD_LEN + 1, masterDownloadDoneCallback, NULL));
    ASSERT_EQ(-CANAS_ERR_BAD_NODE_ID, canasSrvDdsDownloadTo(&inst, CANAS_BROADCAST_NODE_ID, memid, data.c_str(),
        data.length(), masterDownloadDoneCallback, NULL));

    FOR_EACH_IFACE(i)
    {
        EXPECT_EQ(1, iface_send_counter[i]);
        CanasMessage msg = extractCanasMessage(iface_send_dump[i]);
        ASSERT_EQ(remote_node_id, msg.node_id);
        ASSERT_EQ(SERVICE_CODE_DDS, msg.service_code);
        ASSERT_EQ(CANAS_DATATYPE_MEMID, msg.data.type);
        ASSERT_EQ(msgcount, msg.message_code);
        ASSERT_EQ(memid, be32toh(msg.data.container.MEMID));
    }

    // Send the SDRM response back:
    ASSERT_EQ(1, CANAS_SRV_DDS_RESPONSE_XON);              // The line below assumes this.
    CanasCanFrame sdrm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, remote_node_id,
        CANAS_DATATYPE_LONG, SERVICE_CODE_DDS, 0,
        0, 0, 0, 1); // here.

    master_download_done = false;
    ASSERT_EQ(0, canasUpdate(&inst, 0, &sdrm_response));
    ASSERT_FALSE(master_download_done);

    // Due to time step, every call will kick one message to the bus until all the data has transmitted.
    for (int i = 0; i < msgcount; i++)
    {
        current_timestamp += UPDATE_TIME_STEP;
        ASSERT_EQ(0, canasUpdate(&inst, -1, NULL));
    }

    const uint8_t last_message_code = extractCanasMessage(iface_send_dump[0]).message_code;
    ASSERT_GT(last_message_code, 0);                                      // Just in case

    // Transmission shall be done here, it is time to send a checksum back:
    CanasCanFrame checksum_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, remote_node_id,
        CANAS_DATATYPE_CHKSUM, SERVICE_CODE_DDS, 0,
        getbyte(checksum, 3), getbyte(checksum, 2), getbyte(checksum, 1), getbyte(checksum, 0));

    current_timestamp += UPDATE_TIME_STEP;
    ASSERT_EQ(0, canasUpdate(&inst, 1, &checksum_response));

    // Make sure that application got the proper callback:
    ASSERT_TRUE(master_download_done);
    ASSERT_TRUE(master_download_done_args.parg == &inst);
    ASSERT_EQ(remote_node_id,            master_download_done_args.node_id);
    ASSERT_EQ(memid,                     master_download_done_args.memid);
    ASSERT_EQ(CANAS_SRV_DATA_SESSION_OK, master_download_done_args.status);
}

TEST(StdSrvDDS, Reception)
{
    memory_chunk_size_limit = 2048;
    CanasInstance inst = makeGenericInstance();
    ASSERT_EQ(0, canasSrvDataInit(&inst, 1,            // One session slot
        slaveDownloadRequestCallback, slaveDownloadDoneCallback, slaveUploadRequestCallback));

    int msgcount = 2;

    CanasCanFrame sdrm_request = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN, 0, inst.config.node_id,
        CANAS_DATATYPE_MEMID, SERVICE_CODE_DDS, msgcount,
        0xde, 0xad, 0xbe, 0xef);
    const uint32_t memid = 0xdeadbeef;

    CanasCanFrame payload_messages[] =
    {
        makeFrame(CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN, 0, inst.config.node_id, CANAS_DATATYPE_UCHAR4, SERVICE_CODE_DDS, 0,
                'H', 'e', 'l', 'l'),
        makeFrame(CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN, 0, inst.config.node_id, CANAS_DATATYPE_UCHAR2, SERVICE_CODE_DDS, 1,
                'o', '!')
    };
    const int last_message_code = 1;
    const std::string hello = "Hello!";
    const uint32_t checksum = computeChecksum(reinterpret_cast<const uint8_t*>(hello.c_str()), hello.length());

    FOR_EACH_IFACE(i)
    {
        iface_send_return_values[i] = 1;
        iface_send_counter[i] = 0;
    }

    slave_download_request_return_value = CANAS_SRV_DDS_RESPONSE_XOFF;  // XOFF must be silently converted to XON
    slave_download_done = slave_download_requested = false;

    // Send the SDRM request and check if the proper answer was sent back:
    ASSERT_EQ(0, canasUpdate(&inst, 0, &sdrm_request));

    FOR_EACH_IFACE(i)
    {
        ASSERT_EQ(1, iface_send_counter[i]);
        // Check header
        CanasMessage msg = extractCanasMessage(iface_send_dump[i]);
        ASSERT_EQ(inst.config.node_id, msg.node_id);
        ASSERT_EQ(CANAS_DATATYPE_LONG, msg.data.type);
        ASSERT_EQ(SERVICE_CODE_DDS,    msg.service_code);
        ASSERT_EQ(msgcount,            msg.message_code);
        // Check the payload - should be XON
        ASSERT_EQ(CANAS_SRV_DDS_RESPONSE_XON, int32_t(be32toh(msg.data.container.LONG)));
    }

    ASSERT_EQ(memid, slave_download_memid);
    ASSERT_FALSE(slave_download_done);
    ASSERT_TRUE(slave_download_requested);
    slave_download_requested = false;

    // Another SDRM request must return ABORT because there is no free slots available:
    CanasCanFrame sdrm_request_to_discard = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN + 2, 0, inst.config.node_id,
        CANAS_DATATYPE_MEMID, SERVICE_CODE_DDS, msgcount,
        0x44, 0x33, 0x22, 0x11);
    current_timestamp += UPDATE_TIME_STEP;
    ASSERT_EQ(0, canasUpdate(&inst, 0, &sdrm_request_to_discard));
    FOR_EACH_IFACE(i)
    {
        ASSERT_EQ(2, iface_send_counter[i]);
        CanasMessage msg = extractCanasMessage(iface_send_dump[i]);
        ASSERT_EQ(inst.config.node_id, msg.node_id);
        ASSERT_EQ(CANAS_DATATYPE_LONG, msg.data.type);
        ASSERT_EQ(SERVICE_CODE_DDS,    msg.service_code);
        ASSERT_EQ(CANAS_SRV_DDS_RESPONSE_ABORT, int32_t(be32toh(msg.data.container.LONG)));
    }

    // Well, now we send the remaining messages:
    for (int i = 0; i < msgcount; i++)
    {
        current_timestamp += UPDATE_TIME_STEP;
        ASSERT_EQ(0, canasUpdate(&inst, i % IFACE_COUNT, payload_messages + i));  // Using different interfaces
    }

    // Checksum must be emitted:
    FOR_EACH_IFACE(i)
    {
        ASSERT_EQ(3, iface_send_counter[i]);
        CanasMessage msg = extractCanasMessage(iface_send_dump[i]);
        ASSERT_EQ(CANAS_DATATYPE_CHKSUM, msg.data.type);
        ASSERT_EQ(last_message_code,     msg.message_code);
        ASSERT_EQ(checksum, be32toh(msg.data.container.CHKSUM));
    }

    // Check the received data:
    ASSERT_TRUE(slave_download_done);
    ASSERT_FALSE(slave_download_requested);
    ASSERT_EQ(memid, slave_download_memid);
    ASSERT_EQ(hello.length(), slave_download_datalen);
    ASSERT_EQ(0, std::memcmp(hello.c_str(), slave_download_buff, hello.length()));
}

TEST(StdSrvDUS, Reception)
{
    memory_chunk_size_limit = 4096;
    CanasInstance inst = makeGenericInstance();
    ASSERT_EQ(0, canasSrvDataInit(&inst, 2,            // One session slot
        slaveDownloadRequestCallback, slaveDownloadDoneCallback, slaveUploadRequestCallback));

    uint8_t remote_node_id = inst.config.node_id + 1;
    uint32_t memid = 0xdeadbeef;
    const std::string the_cat = "The cat";
    const uint32_t checksum = computeChecksum(reinterpret_cast<const uint8_t*>(the_cat.c_str()), the_cat.length());
    const int msgcount = msgcountByDatalen(the_cat.length());
    ASSERT_EQ(2, msgcount);

    CanasCanFrame payload_messages[] =
    {
        makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, remote_node_id, CANAS_DATATYPE_UCHAR4, SERVICE_CODE_DUS, 0,
                'T', 'h', 'e', ' '),
        makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, remote_node_id, CANAS_DATATYPE_UCHAR3, SERVICE_CODE_DUS, 1,
                'c', 'a', 't')
    };
    const int last_message_code = 1;

    FOR_EACH_IFACE(i)
    {
        iface_send_return_values[i] = 1;
        iface_send_counter[i] = 0;
    }

    master_upload_done = false;

    // Init transfer. This should produce the SURM request on wire:
    ASSERT_EQ(0, canasSrvDusUploadFrom(&inst, remote_node_id, memid, the_cat.length(), masterUploadDoneCallback, &inst));
    ASSERT_EQ(-CANAS_ERR_ENTRY_EXISTS,
              canasSrvDusUploadFrom(&inst, remote_node_id, memid, the_cat.length(), masterUploadDoneCallback, &inst));

    // Check the request message:
    FOR_EACH_IFACE(i)
    {
        ASSERT_EQ(1, iface_send_counter[i]);
        CanasMessage msg = extractCanasMessage(iface_send_dump[i]);
        ASSERT_EQ(remote_node_id, msg.node_id);
        ASSERT_EQ(SERVICE_CODE_DUS, msg.service_code);
        ASSERT_EQ(CANAS_DATATYPE_MEMID, msg.data.type);
        ASSERT_EQ(msgcount, msg.message_code);
        ASSERT_EQ(memid, be32toh(msg.data.container.MEMID));
    }

    // Send SURM response:
    CanasCanFrame surm_response = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, remote_node_id,
        CANAS_DATATYPE_LONG, SERVICE_CODE_DUS, 0,
        0, 0, 0, 0);
    ASSERT_EQ(0, CANAS_SRV_DUS_RESPONSE_OK);      // The line above assumes this
    current_timestamp += UPDATE_TIME_STEP;
    ASSERT_EQ(0, canasUpdate(&inst, 0, &surm_response));

    // Kick all messages to the master:
    for (int i = 0; i < msgcount; i++)
    {
        current_timestamp += UPDATE_TIME_STEP;
        ASSERT_EQ(0, canasUpdate(&inst, i % IFACE_COUNT, payload_messages + i));
    }

    ASSERT_FALSE(master_upload_done);

    // Don't forget the checksum:
    CanasCanFrame checksum_message = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_HIGH_MIN + 1, 0, remote_node_id,
        CANAS_DATATYPE_CHKSUM, SERVICE_CODE_DUS, last_message_code,
        getbyte(checksum, 3), getbyte(checksum, 2), getbyte(checksum, 1), getbyte(checksum, 0));
    current_timestamp += UPDATE_TIME_STEP;
    ASSERT_EQ(0, canasUpdate(&inst, 0, &checksum_message));

    // No additional trsnsmissions should occur:
    FOR_EACH_IFACE(i)
        ASSERT_EQ(1, iface_send_counter[i]);

    ASSERT_TRUE(master_upload_done);
    ASSERT_EQ(CANAS_SRV_DATA_SESSION_OK, master_upload_done_args.status);
    ASSERT_EQ(the_cat.length(), master_upload_done_args.datalen);
    ASSERT_EQ(memid,            master_upload_done_args.memid);
    ASSERT_EQ(remote_node_id,   master_upload_done_args.node_id);
    ASSERT_TRUE(&inst == master_upload_done_args.parg);
    ASSERT_EQ(0, memcmp(master_upload_buff, the_cat.c_str(), master_upload_done_args.datalen));
}

TEST(StdSrvDUS, Transmission)
{
    memory_chunk_size_limit = 2048;
    CanasInstance inst = makeGenericInstance();
    ASSERT_EQ(0, canasSrvDataInit(&inst, 1,            // One session slot
        slaveDownloadRequestCallback, slaveDownloadDoneCallback, slaveUploadRequestCallback));

    const std::string data = "Strictly no elephants.";
    const uint32_t checksum = computeChecksum(reinterpret_cast<const uint8_t*>(data.c_str()), data.length());
    const int msgcount = msgcountByDatalen(data.length());
    const int last_message_code = msgcount - 1;

    FOR_EACH_IFACE(i)
    {
        iface_send_return_values[i] = 1;
        iface_send_counter[i] = 0;
    }
    slave_upload_requested = false;
    slave_upload_request_return_value = CANAS_SRV_DUS_RESPONSE_OK;
    slave_upload_pointer = reinterpret_cast<const uint8_t*>(data.c_str());
    slave_upload_datalen = data.length();

    // Feed the remote SURM request:
    CanasCanFrame surm_request = makeFrame(CANAS_MSGTYPE_NODE_SERVICE_LOW_MIN, 0, inst.config.node_id,
        CANAS_DATATYPE_MEMID, SERVICE_CODE_DUS, msgcount,
        0xde, 0xad, 0xbe, 0xef);
    const uint32_t memid = 0xdeadbeef;
    ASSERT_EQ(0, canasUpdate(&inst, 0, &surm_request));

    // Check that application callback was issued:
    ASSERT_TRUE(slave_upload_requested);
    ASSERT_EQ(memid, slave_upload_memid);

    // Check if the proper response was sent:
    FOR_EACH_IFACE(i)
    {
        ASSERT_EQ(1, iface_send_counter[i]);
        iface_send_counter[i] = 0;                                       // Reset the counter for the next check
        CanasMessage msg = extractCanasMessage(iface_send_dump[i]);
        ASSERT_EQ(inst.config.node_id, msg.node_id);
        ASSERT_EQ(SERVICE_CODE_DUS,    msg.service_code);
        ASSERT_EQ(CANAS_DATATYPE_LONG, msg.data.type);
        ASSERT_EQ(msgcount,            msg.message_code);
        ASSERT_EQ(CANAS_SRV_DUS_RESPONSE_OK, int32_t(be32toh(msg.data.container.LONG)));
    }

    for (int i = 0; i < (msgcount + 2); i++)
    {
        current_timestamp += UPDATE_TIME_STEP;
        ASSERT_EQ(0, canasUpdate(&inst, 0, NULL));
    }

    FOR_EACH_IFACE(i)
    {
        ASSERT_EQ(msgcount + 1, iface_send_counter[i]);
        CanasMessage msg = extractCanasMessage(iface_send_dump[i]);     // Last message is the checksum
        ASSERT_EQ(inst.config.node_id,   msg.node_id);
        ASSERT_EQ(SERVICE_CODE_DUS,      msg.service_code);
        ASSERT_EQ(CANAS_DATATYPE_CHKSUM, msg.data.type);
        ASSERT_EQ(last_message_code,     msg.message_code);
        ASSERT_EQ(checksum, be32toh(msg.data.container.CHKSUM));
    }
}
