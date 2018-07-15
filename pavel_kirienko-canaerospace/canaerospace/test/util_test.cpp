/*
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include "test.hpp"

TEST(DumpTest, CanFrameBufSpace)
{
    char buf[CANAS_DUMP_BUF_LEN * 50];
    std::memset(buf, 0xaa, sizeof(buf));

    CanasCanFrame frm = makeFrame(1234, 255, 0, CANAS_DATATYPE_UDEF_BEGIN_, 0, 0, 0xde, 0xad, 0xbe, 0xef);
    canasDumpCanFrame(&frm, buf);

    const int len = std::strlen(buf);
    std::cout << "frame dump [" << std::dec << len << "]: " << buf << std::endl;

    EXPECT_LT(len, CANAS_DUMP_BUF_LEN - 2) <<
        "Possible overflow in canasDumpCanFrame(), buffer size needs to be increased";
}

TEST(DumpTest, MessageBufSpace)
{
    char buf[CANAS_DUMP_BUF_LEN * 50];
    std::memset(buf, 0xaa, sizeof(buf));

    CanasMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.data.length = 4;
    msg.data.type = CANAS_DATATYPE_UDEF_BEGIN_;
    canasDumpMessage(&msg, buf);

    const int len = std::strlen(buf);
    std::cout << "message dump [" << std::dec << len << "]: " << buf << std::endl;

    EXPECT_LT(len, CANAS_DUMP_BUF_LEN - 2) <<
        "Possible overflow in canasDumpMessage(), buffer size needs to be increased";
}
