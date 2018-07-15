/*
 * Tests of data conversion
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include "test.hpp"
#include "../src/marshal.h"

class Container
{
    static const int BUFLEN = 32;
    uint8_t buf_[BUFLEN];

public:
    Container() { reset(); }

    void reset() { std::memset(buf_, 0xaa, BUFLEN); }

    void validate(int byte0 = -1, int byte1 = -1, int byte2 = -1, int byte3 = -1)
    {
        int i = 0;
        for (i = 0; i < BUFLEN / 2; i++)
            EXPECT_EQ(0xaa, buf_[i]);

#define VALIDATE(b) \
    if (GLUE(byte, b) >= 0) { \
        EXPECT_LT(GLUE(byte, b), 256) << "Invalid usage of validate()"; \
        EXPECT_EQ(GLUE(byte, b), buf_[i++]); \
    }
        VALIDATE(0);
        VALIDATE(1);
        VALIDATE(2);
        VALIDATE(3);
#undef VALIDATE

        for (; i < BUFLEN; i++)
            EXPECT_EQ(0xaa, buf_[i]);

        reset();
    }

    operator uint8_t* () { return buf_ + BUFLEN / 2; }

    uint8_t& operator [](int index)
    {
        EXPECT_GE(index, 0);
        EXPECT_LT(index, BUFLEN / 2);
        return buf_[BUFLEN / 2 + index];
    }
};

TEST(MarshalTest, HostToNetworkErrors)
{
    Container cont;
    CanasMessageData msgd;
    std::memset(&msgd, 0xaa, sizeof(msgd));

    msgd.type = CANAS_DATATYPE_RESVD_BEGIN_;  // Wrong type
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, canasHostToNetwork(cont, &msgd));

    msgd.type = CANAS_DATATYPE_UDEF_END_;  // Correct type
    msgd.length = 5;                       // Bad length
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, canasHostToNetwork(cont, &msgd));

    // Invalid arguments:
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasHostToNetwork(NULL, &msgd));
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasHostToNetwork(cont, NULL));
}

TEST(MarshalTest, HostToNetwork)
{
    Container cont;
    CanasMessageData msgd;
    std::memset(&msgd, 0xaa, sizeof(msgd));

    msgd.container.ULONG = 0xdeadbeef;
    msgd.type = CANAS_DATATYPE_ULONG;
    msgd.length = 123;   // Must be ignored
    EXPECT_EQ(4, canasHostToNetwork(cont, &msgd));
    cont.validate(0xde, 0xad, 0xbe, 0xef);

    msgd.container.USHORT2[0] = 0xcafe;
    msgd.container.USHORT2[1] = 0xface;
    msgd.type = CANAS_DATATYPE_USHORT2;
    EXPECT_EQ(4, canasHostToNetwork(cont, &msgd));
    cont.validate(0xca, 0xfe, 0xfa, 0xce);

    msgd.type = CANAS_DATATYPE_UDEF_BEGIN_;
    msgd.length = 3;
    msgd.container.UCHAR4[0] = 12;
    msgd.container.UCHAR4[1] = 34;
    msgd.container.UCHAR4[2] = 56;
    msgd.container.UCHAR4[3] = 78;  // This will be ignored
    EXPECT_EQ(3, canasHostToNetwork(cont, &msgd));
    cont.validate(12, 34, 56);

    msgd.type = CANAS_DATATYPE_UDEF_END_;
    msgd.length = 0;       // No data
    msgd.container.LONG = 0x1deface1;
    EXPECT_EQ(0, canasHostToNetwork(cont, &msgd));
    cont.validate();
}

TEST(MarshalTest, NetworkToHostErrors)
{
    Container cont;
    CanasMessageData msgd;
    std::memset(&msgd, 0xaa, sizeof(msgd));

    // Invalid pointer:
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasNetworkToHost(&msgd, NULL, 0, CANAS_DATATYPE_NODATA));
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasNetworkToHost(NULL, cont, 0, CANAS_DATATYPE_NODATA));

    // Wrong data length:
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, canasNetworkToHost(&msgd, cont, 5, CANAS_DATATYPE_NODATA));

    // Wrong data type:
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, canasNetworkToHost(&msgd, cont, 0, CANAS_DATATYPE_RESVD_BEGIN_));
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, canasNetworkToHost(&msgd, cont, 4, CANAS_DATATYPE_RESVD_END_));

    // Mismatched data type and length:
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, canasNetworkToHost(&msgd, cont, 3, CANAS_DATATYPE_BLONG));
    EXPECT_EQ(-CANAS_ERR_BAD_DATA_TYPE, canasNetworkToHost(&msgd, cont, 2, CANAS_DATATYPE_CHAR));
}

TEST(MarshalTest, NetworkToHost)
{
    Container cont;
    CanasMessageData msgd;
    std::memset(&msgd, 0xaa, sizeof(msgd));

    cont[0] = 0xde;
    cont[1] = 0xad;
    cont[2] = 0xbe;
    cont[3] = 0xef;
    EXPECT_EQ(4, canasNetworkToHost(&msgd, cont, 4, CANAS_DATATYPE_ULONG));
    EXPECT_EQ(0xdeadbeef, msgd.container.ULONG);

    std::memset(&msgd, 0xaa, sizeof(msgd));
    EXPECT_EQ(3, canasNetworkToHost(&msgd, cont, 3, CANAS_DATATYPE_UDEF_BEGIN_));
    EXPECT_EQ(0xde, msgd.container.UCHAR3[0]);
    EXPECT_EQ(0xad, msgd.container.UCHAR3[1]);
    EXPECT_EQ(0xbe, msgd.container.UCHAR3[2]);
    EXPECT_EQ(0x00, msgd.container.UCHAR3[3]);  // This byte is unused. Thus, it MUST be initialized to zero.
}
