/*
 * Tests for Generic Redundancy Resolver
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <math.h>
#include <float.h>
#include <canaerospace/generic_redundancy_resolver.h>
#include "test.hpp"

TEST(GrrTest, Generic)
{
    CanasInstance inst = makeGenericInstance();

    CanasGrrConfig cfg = canasGrrMakeConfig();
    cfg.channel_timeout_usec = 20000;
    cfg.fom_hysteresis = 0.2;
    cfg.min_fom_switch_interval_usec = 500000;
    cfg.num_channels = 3;

    float fom = 0.f;
    uint64_t timestamp = 0;

    CanasGrrInstance grr;
    EXPECT_EQ(0, canasGrrInit(&grr, &cfg, &inst));

    // Initial state
    EXPECT_EQ(0, canasGrrGetActiveChannel(&grr));
    EXPECT_EQ(0, canasGrrGetLastSwitchTimestamp(&grr, &timestamp));
    EXPECT_EQ(0, timestamp);

    for (int i = 0; i < 3; i++)
    {
        EXPECT_EQ(0, canasGrrGetChannelState(&grr, i, &fom, &timestamp));
        EXPECT_EQ(fom, 0.0f);
        EXPECT_EQ(0, timestamp);
    }

    // First update
    EXPECT_EQ(CANAS_GRR_REASON_INIT,   canasGrrUpdate(&grr, 0, 1.0, 100));
    EXPECT_EQ(0, canasGrrGetActiveChannel(&grr));
    EXPECT_EQ(0, canasGrrGetLastSwitchTimestamp(&grr, &timestamp));
    EXPECT_EQ(100, timestamp);

    // FOM comparison with hysteresis and min switching interval constraint
    EXPECT_EQ(CANAS_GRR_REASON_NONE,   canasGrrUpdate(&grr, 0, 1.0, 200));
    EXPECT_EQ(CANAS_GRR_REASON_NONE,   canasGrrUpdate(&grr, 1, 2.0, 300));       // Min interval constraint
    EXPECT_EQ(CANAS_GRR_REASON_NONE,   canasGrrUpdate(&grr, 0, 0.0, 1000000));
    EXPECT_EQ(CANAS_GRR_REASON_NONE,   canasGrrUpdate(&grr, 1, 0.1, 1000100));
    EXPECT_EQ(0, canasGrrGetActiveChannel(&grr));
    EXPECT_EQ(0, canasGrrGetLastSwitchTimestamp(&grr, &timestamp));
    EXPECT_EQ(100, timestamp);

    // Switching
    EXPECT_EQ(CANAS_GRR_REASON_FOM,    canasGrrUpdate(&grr, 1, 0.3, 1000200));
    EXPECT_EQ(CANAS_GRR_REASON_TIMEOUT,canasGrrUpdate(&grr, 2, 0.3, 1600000));
    EXPECT_EQ(2, canasGrrGetActiveChannel(&grr));
    EXPECT_EQ(0, canasGrrGetLastSwitchTimestamp(&grr, &timestamp));
    EXPECT_EQ(1600000, timestamp);

    // NAN FOM
    EXPECT_EQ(CANAS_GRR_REASON_NONE,   canasGrrUpdate(&grr, 2, NAN, 3000000)); // NAN will be translated into -FLT_MAX
    EXPECT_EQ(CANAS_GRR_REASON_FOM,    canasGrrUpdate(&grr, 0, -1., 3000100)); // Any finite FOM is preferred over NAN
    EXPECT_EQ(0, canasGrrGetActiveChannel(&grr));
    EXPECT_EQ(0, canasGrrGetLastSwitchTimestamp(&grr, &timestamp));
    EXPECT_EQ(3000100, timestamp);

    // Manual override
    EXPECT_EQ(0, canasGrrOverrideActiveChannel(&grr, 2));
    EXPECT_EQ(2, canasGrrGetActiveChannel(&grr));
    EXPECT_EQ(CANAS_GRR_REASON_TIMEOUT,canasGrrUpdate(&grr, 0, -1., 4000000));
    EXPECT_EQ(CANAS_GRR_REASON_NONE,   canasGrrUpdate(&grr, 1, 0.3, 4000100)); // Min interval constraint
    EXPECT_EQ(CANAS_GRR_REASON_TIMEOUT,canasGrrUpdate(&grr, 1, 1.0, 5000000)); // Timeout has higher priority than FOM
    EXPECT_EQ(1, canasGrrGetActiveChannel(&grr));
    EXPECT_EQ(0, canasGrrGetLastSwitchTimestamp(&grr, &timestamp));
    EXPECT_EQ(5000000, timestamp);

    // Channel state
    EXPECT_EQ(0, canasGrrGetChannelState(&grr, 0, &fom, &timestamp));
    EXPECT_EQ(fom, -1.f);
    EXPECT_EQ(4000000, timestamp);

    EXPECT_EQ(0, canasGrrGetChannelState(&grr, 1, &fom, &timestamp));
    EXPECT_EQ(fom, 1.0);
    EXPECT_EQ(5000000, timestamp);

    EXPECT_EQ(0, canasGrrGetChannelState(&grr, 2, &fom, &timestamp));
    EXPECT_EQ(-FLT_MAX, fom);        // Make sure NAN was translated
    EXPECT_EQ(3000000, timestamp);
}

TEST(GrrTest, ForcedChannelInitialization)
{
    CanasInstance inst = makeGenericInstance();

    CanasGrrConfig cfg = canasGrrMakeConfig();
    cfg.channel_timeout_usec = 20000;
    cfg.fom_hysteresis = 0.2;
    cfg.min_fom_switch_interval_usec = 500000;
    cfg.num_channels = 3;

    CanasGrrInstance grr;
    EXPECT_EQ(0, canasGrrInit(&grr, &cfg, &inst));
    EXPECT_EQ(0, canasGrrOverrideActiveChannel(&grr, 2));
    EXPECT_EQ(2, canasGrrGetActiveChannel(&grr));
    EXPECT_EQ(CANAS_GRR_REASON_NONE, canasGrrUpdate(&grr, 2, 0.0, 100)); // In this case first call will return NONE
    EXPECT_EQ(2, canasGrrGetActiveChannel(&grr));
}

TEST(GrrTest, Validation)
{
    CanasInstance inst = makeGenericInstance();

    const size_t initial_mem_chunks = mem_chunks.size();

    CanasGrrConfig cfg = canasGrrMakeConfig();
    CanasGrrInstance grr;

    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrInit(&grr, &cfg, &inst));

    cfg.channel_timeout_usec = 20000;
    cfg.fom_hysteresis = 0.2;
    cfg.min_fom_switch_interval_usec = 500000;
    cfg.num_channels = 3;

    EXPECT_EQ(0, canasGrrInit(&grr, &cfg, &inst));
    EXPECT_EQ(0, canasGrrDispose(&grr));
    EXPECT_EQ(initial_mem_chunks, mem_chunks.size());      // GRR failed to deallocate the memory
    EXPECT_EQ(0, canasGrrDispose(&grr));                   // Making sure it is safe to dispose twice
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrDispose(NULL));

    cfg.fom_hysteresis = 0.0;
    cfg.min_fom_switch_interval_usec = 0;
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrInit(&grr, &cfg, &inst)); // Neither hysteresis nor time constraint are set

    cfg.fom_hysteresis = 0.2;
    cfg.min_fom_switch_interval_usec = 500000;
    EXPECT_EQ(0, canasGrrInit(&grr, &cfg, &inst));

    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrGetActiveChannel(NULL));
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrGetLastSwitchTimestamp(&grr, NULL));
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrGetChannelState(&grr, 3, NULL, NULL)); // Channel index out of range

    EXPECT_EQ(0, canasGrrGetChannelState(&grr, 0, NULL, NULL)); // Output args are optional!
    EXPECT_EQ(0, canasGrrGetChannelState(&grr, 2, NULL, NULL));

    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrOverrideActiveChannel(NULL, 0));
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrOverrideActiveChannel(&grr, 3)); // Channel index out of range

    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrUpdate(&grr, 3, 0.0, 1)); // Channel index out of range
    EXPECT_EQ(-CANAS_ERR_ARGUMENT, canasGrrUpdate(&grr, 1, 0.0, 0)); // Invalid timestamp
}
