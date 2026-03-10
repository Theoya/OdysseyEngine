#include <gtest/gtest.h>
#include "vulkan/command.h"

using odyssey::vulkan::compute_dispatch_config;

// ---------------------------------------------------------------------------
// compute_dispatch_config is a pure function:
//   DispatchConfig compute_dispatch_config(uint32_t entity_count,
//                                          uint32_t workgroup_size);
//   DispatchConfig { group_count_x, group_count_y, group_count_z };
// group_count_x = ceil(entity_count / workgroup_size)
// group_count_y = 1,  group_count_z = 1  (1-D dispatches for Nadir)
// ---------------------------------------------------------------------------

TEST(DispatchConfig, ExactMultiple) {
    auto config = compute_dispatch_config(256, 256);
    EXPECT_EQ(config.group_count_x, 1u);
    EXPECT_EQ(config.group_count_y, 1u);
    EXPECT_EQ(config.group_count_z, 1u);
}

TEST(DispatchConfig, RoundsUp) {
    // ceil(1000 / 256) = 4
    auto config = compute_dispatch_config(1000, 256);
    EXPECT_EQ(config.group_count_x, 4u);
}

TEST(DispatchConfig, SingleEntity) {
    auto config = compute_dispatch_config(1, 256);
    EXPECT_EQ(config.group_count_x, 1u);
}

TEST(DispatchConfig, ZeroEntities) {
    auto config = compute_dispatch_config(0, 256);
    EXPECT_EQ(config.group_count_x, 0u);
}

TEST(DispatchConfig, LargeCount) {
    // ceil(100000 / 256) = 391
    auto config = compute_dispatch_config(100000, 256);
    EXPECT_EQ(config.group_count_x, 391u);
}

TEST(DispatchConfig, SmallWorkgroup) {
    // ceil(100 / 64) = 2
    auto config = compute_dispatch_config(100, 64);
    EXPECT_EQ(config.group_count_x, 2u);
}

TEST(DispatchConfig, WorkgroupSize1) {
    auto config = compute_dispatch_config(10, 1);
    EXPECT_EQ(config.group_count_x, 10u);
}

TEST(DispatchConfig, YZAlwaysOne) {
    auto config = compute_dispatch_config(5000, 256);
    EXPECT_EQ(config.group_count_y, 1u);
    EXPECT_EQ(config.group_count_z, 1u);
}

TEST(DispatchConfig, PowerOfTwoBoundary) {
    // 512 / 256 = 2 exactly
    auto config = compute_dispatch_config(512, 256);
    EXPECT_EQ(config.group_count_x, 2u);
}

TEST(DispatchConfig, OneOverBoundary) {
    // 257 / 256 => ceil = 2
    auto config = compute_dispatch_config(257, 256);
    EXPECT_EQ(config.group_count_x, 2u);
}

TEST(DispatchConfig, OneUnderBoundary) {
    // 255 / 256 => ceil = 1
    auto config = compute_dispatch_config(255, 256);
    EXPECT_EQ(config.group_count_x, 1u);
}
