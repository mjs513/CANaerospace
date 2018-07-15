/*
 * Shared stuff
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#ifndef TEST_HPP_
#define TEST_HPP_

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <set>
#include <vector>
#include <gtest/gtest.h>
#include <canaerospace/canaerospace.h>
// These inclusions are only for syntax checking:
#include <canaerospace/param_id/nod_default.h>
#include <canaerospace/param_id/uav.h>

#define DEFAULT_MEM_CHUNK_SIZE_LIMIT 1024
#define IFACE_COUNT        3
#define FILTERS_PER_IFACE  2
#define MY_NODE_ID         42

#define CHECKPTR(x) EXPECT_FALSE(x == NULL) << "Invalid pointer"

#define GLUE_(a, b) a##b
#define GLUE(a, b) GLUE_(a, b)

#define FOR_EACH_IFACE(i) for (int i = 0; i < IFACE_COUNT; i++)

namespace
{
    template <typename T> int getbyte(T v, int byte) { return (v >> (byte * 8)) & 0xFF; }

    int memory_chunk_size_limit = 1024;
    typedef std::set<void*> MemChunks;
    MemChunks mem_chunks;

    void* malloc_(CanasInstance* pi, int size)
    {
        CHECKPTR(pi);
        EXPECT_GT(size, 0);
        if (size > memory_chunk_size_limit)
        {
            std::cout << std::dec << size << " bytes is way too much, isn't it?" << std::endl;
            return NULL;
        }
        void* ptr = std::malloc(size);
        std::cout << "MALLOC " << std::dec << size << " bytes @ " << std::hex << ptr << std::endl;
        CHECKPTR(ptr);
        EXPECT_TRUE(mem_chunks.insert(ptr).second);
        return ptr;
    }

    void free_(CanasInstance* pi, void* ptr)
    {
        CHECKPTR(pi);
        CHECKPTR(ptr);
        std::cout << "FREE @ " << std::hex << ptr << std::endl;
        EXPECT_EQ(1, mem_chunks.erase(ptr)) << "Attempt to deallocate a nonexistent chunk of memory [ptr=0x"
            << std::hex << ptr << "]";
        return std::free(ptr);
    }

    void resetMemory()
    {
        memory_chunk_size_limit = DEFAULT_MEM_CHUNK_SIZE_LIMIT;
        for (MemChunks::iterator it = mem_chunks.begin(); it != mem_chunks.end(); it++)
            std::free(*it);
        mem_chunks.clear();
    }

    uint64_t current_timestamp = 1;

    uint64_t getTimestamp(CanasInstance*)
    {
        return current_timestamp;
    }

    int iface_send_return_values[IFACE_COUNT];
    int iface_filter_return_value = 0;
    CanasCanFrame iface_send_dump[IFACE_COUNT];
    int iface_send_counter[IFACE_COUNT];
    std::vector<CanasCanFilterConfig> iface_filters[IFACE_COUNT];

    int drvSend(CanasInstance* pi, int iface, const CanasCanFrame* pframe)
    {
        CHECKPTR(pi);
        CHECKPTR(pframe);
        EXPECT_GE(iface, 0);
        EXPECT_LE(iface, IFACE_COUNT);
        iface_send_dump[iface] = *pframe;
        iface_send_counter[iface]++;
        return iface_send_return_values[iface];
    }

    int drvFilter(CanasInstance* pi, int iface, const CanasCanFilterConfig* pfilters, int nfilters)
    {
        CHECKPTR(pi);
        CHECKPTR(pfilters);
        EXPECT_GT(nfilters, 0);
        EXPECT_GE(iface, 0);
        EXPECT_LE(iface, IFACE_COUNT);
        iface_filters[iface].clear();
        for (int i = 0; i < nfilters; i++)
            iface_filters[iface].push_back(pfilters[i]);
        return iface_filter_return_value;
    }

    CanasHookCallbackArgs cbargs_hook;
    CanasParamCallbackArgs cbargs_param;
    CanasServicePollCallbackArgs cbargs_srv_poll;
    CanasServiceRequestCallbackArgs cbargs_srv_request;
    CanasServiceResponseCallbackArgs cbargs_srv_response;

    int cbcnt_hook = 0;
    int cbcnt_param = 0;
    int cbcnt_srv_poll = 0;
    int cbcnt_srv_request = 0;
    int cbcnt_srv_response = 0;

    void cbHook(CanasInstance* pi, CanasHookCallbackArgs* pargs)
    {
        CHECKPTR(pi);
        CHECKPTR(pargs);
        cbargs_hook = *pargs;
        cbcnt_hook++;
    }

    void cbParam(CanasInstance* pi, CanasParamCallbackArgs* pargs)
    {
        CHECKPTR(pi);
        CHECKPTR(pargs);
        cbargs_param = *pargs;
        cbcnt_param++;
    }

    void cbSrvPoll(CanasInstance* pi, CanasServicePollCallbackArgs* pargs)
    {
        CHECKPTR(pi);
        CHECKPTR(pargs);
        cbargs_srv_poll = *pargs;
        cbcnt_srv_poll++;
    }

    void cbSrvRequest(CanasInstance* pi, CanasServiceRequestCallbackArgs* pargs)
    {
        CHECKPTR(pi);
        CHECKPTR(pargs);
        cbargs_srv_request = *pargs;
        cbcnt_srv_request++;
    }

    void cbSrvResponse(CanasInstance* pi, CanasServiceResponseCallbackArgs* pargs)
    {
        CHECKPTR(pi);
        CHECKPTR(pargs);
        cbargs_srv_response = *pargs;
        cbcnt_srv_response++;
    }

    CanasConfig makeGenericConfig()
    {
        CanasConfig cfg = canasMakeConfig();

        cfg.fn_send = drvSend;
        cfg.fn_filter = drvFilter;
        cfg.fn_timestamp = getTimestamp;
        cfg.fn_malloc = malloc_;
        cfg.fn_free = free_;
        cfg.fn_hook = cbHook;

        cfg.filters_per_iface = FILTERS_PER_IFACE;
        cfg.iface_count = IFACE_COUNT;
        cfg.node_id = MY_NODE_ID;

        return cfg;
    }

    CanasInstance makeGenericInstance()
    {
        CanasConfig cfg = makeGenericConfig();
        CanasInstance inst;
        EXPECT_EQ(0, canasInit(&inst, &cfg, NULL));
        return inst;
    }

    CanasCanFrame makeFrame(uint16_t msg_id, uint8_t redund_chan_id,
                            uint8_t node_id, uint8_t data_type, uint8_t srv_code, uint8_t msg_code,
                            int payload0 = -1, int payload1 = -1, int payload2 = -1, int payload3 = -1)
    {
        CanasCanFrame frm;
        std::memset(&frm, 0, sizeof(frm));
        frm.id = msg_id;
        if (redund_chan_id)
            frm.id |= (((uint32_t)redund_chan_id) * 65536ul) | CANAS_CAN_FLAG_EFF;

        frm.data[0] = node_id;
        frm.data[1] = data_type;
        frm.data[2] = srv_code;
        frm.data[3] = msg_code;
        frm.dlc = 4;
#define ADDPAYLOAD(idx) \
    if (GLUE(payload, idx) >= 0) { \
        EXPECT_LE(GLUE(payload, idx), 255); \
        frm.data[4 + idx] = uint8_t(GLUE(payload, idx)); \
        frm.dlc++; \
    } else return frm;

        ADDPAYLOAD(0);
        ADDPAYLOAD(1);
        ADDPAYLOAD(2);
        ADDPAYLOAD(3);
#undef ADDPAYLOAD
        return frm;
    }

    CanasMessage extractCanasMessage(CanasCanFrame& frame)
    {
        EXPECT_GE(frame.dlc, 4);
        CanasMessage msg;
        std::memset(&msg, 0, sizeof(msg));
        msg.node_id      = frame.data[0];
        msg.data.type    = frame.data[1];
        msg.service_code = frame.data[2];
        msg.message_code = frame.data[3];
        msg.data.length = frame.dlc - 4;
        std::memcpy(msg.data.container.UCHAR4, frame.data + 4, msg.data.length);
        return msg;
    }

    int _canasUpdateWithTimestamp(CanasInstance* pi, int iface, const CanasCanFrame* pframe, uint64_t timestamp_usec)
    {
        current_timestamp = timestamp_usec;
        return canasUpdate(pi, iface, pframe);
    }
}

#endif
