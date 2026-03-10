#include <gtest/gtest.h>
#include "nadir/nadir_buffers.h"

using odyssey::nadir::compute_buffer_set_layout;
using odyssey::nadir::ArchetypeBufferDesc;
using odyssey::nadir::BufferSetLayout;

// ---------------------------------------------------------------------------
// compute_buffer_set_layout is a pure function that maps an
// ArchetypeBufferDesc to byte-level buffer sizes for GPU allocation.
// ---------------------------------------------------------------------------

TEST(BufferSetLayout, BasicSizes) {
    ArchetypeBufferDesc desc{
        .entity_count      = 1024,
        .needs_spatial_grid = true,
        .needs_debug_output = true
    };
    auto layout = compute_buffer_set_layout(desc);

    // Transform: 1024 * POSITION_STRIDE (16)
    EXPECT_EQ(layout.transform_size, 1024u * BufferSetLayout::POSITION_STRIDE);

    // Stats: 1024 * STATS_STRIDE (32)
    EXPECT_EQ(layout.stats_size, 1024u * BufferSetLayout::STATS_STRIDE);

    // Output: 1024 * OUTPUT_STRIDE (64)
    EXPECT_EQ(layout.output_size, 1024u * BufferSetLayout::OUTPUT_STRIDE);

    // Persist: 1024 * PERSIST_STRIDE (64)
    EXPECT_EQ(layout.persist_size, 1024u * BufferSetLayout::PERSIST_STRIDE);

    // Debug: 1024 * DEBUG_STRIDE (16)
    EXPECT_EQ(layout.debug_size, 1024u * BufferSetLayout::DEBUG_STRIDE);

    // Total = sum of all individual buffer sizes
    EXPECT_EQ(layout.total_size,
              layout.transform_size + layout.stats_size +
              layout.spatial_size   + layout.world_state_size +
              layout.persist_size   + layout.output_size +
              layout.debug_size);
}

TEST(BufferSetLayout, ZeroEntities) {
    ArchetypeBufferDesc desc{.entity_count = 0};
    auto layout = compute_buffer_set_layout(desc);
    EXPECT_EQ(layout.transform_size, 0u);
    EXPECT_EQ(layout.stats_size, 0u);
    EXPECT_EQ(layout.output_size, 0u);
    EXPECT_EQ(layout.persist_size, 0u);
}

TEST(BufferSetLayout, NoDebugNoSpatial) {
    ArchetypeBufferDesc desc{
        .entity_count      = 100,
        .needs_spatial_grid = false,
        .needs_debug_output = false
    };
    auto layout = compute_buffer_set_layout(desc);
    EXPECT_EQ(layout.debug_size, 0u);
    EXPECT_EQ(layout.spatial_size, 0u);
}

TEST(BufferSetLayout, SingleEntity) {
    ArchetypeBufferDesc desc{
        .entity_count      = 1,
        .needs_spatial_grid = true,
        .needs_debug_output = true
    };
    auto layout = compute_buffer_set_layout(desc);
    EXPECT_EQ(layout.transform_size, BufferSetLayout::POSITION_STRIDE);
    EXPECT_EQ(layout.stats_size, BufferSetLayout::STATS_STRIDE);
    EXPECT_EQ(layout.output_size, BufferSetLayout::OUTPUT_STRIDE);
    EXPECT_GT(layout.total_size, 0u);
}

TEST(BufferSetLayout, WorldStateConstant) {
    // WorldState size is per-frame, not per-entity, so it must be
    // the same regardless of entity_count.
    ArchetypeBufferDesc desc1{.entity_count = 1};
    ArchetypeBufferDesc desc2{.entity_count = 10000};
    auto layout1 = compute_buffer_set_layout(desc1);
    auto layout2 = compute_buffer_set_layout(desc2);
    EXPECT_EQ(layout1.world_state_size, layout2.world_state_size);
    EXPECT_GT(layout1.world_state_size, 0u);
}

TEST(BufferSetLayout, LargeEntityCount) {
    ArchetypeBufferDesc desc{
        .entity_count      = 100000,
        .needs_spatial_grid = true,
        .needs_debug_output = true
    };
    auto layout = compute_buffer_set_layout(desc);
    EXPECT_EQ(layout.transform_size, 100000u * BufferSetLayout::POSITION_STRIDE);
    EXPECT_GT(layout.total_size, layout.transform_size);
}

TEST(BufferSetLayout, TotalIsNonDecreasing) {
    // More entities should never produce a smaller total.
    ArchetypeBufferDesc small_desc{.entity_count = 10, .needs_spatial_grid = true, .needs_debug_output = true};
    ArchetypeBufferDesc large_desc{.entity_count = 1000, .needs_spatial_grid = true, .needs_debug_output = true};
    auto small_layout = compute_buffer_set_layout(small_desc);
    auto large_layout = compute_buffer_set_layout(large_desc);
    EXPECT_GE(large_layout.total_size, small_layout.total_size);
}

TEST(BufferSetLayout, StrideConstants) {
    // Validate that stride constants match struct expectations.
    EXPECT_EQ(BufferSetLayout::POSITION_STRIDE, 16u);  // vec4
    EXPECT_EQ(BufferSetLayout::STATS_STRIDE, 32u);     // EntityStats
    EXPECT_EQ(BufferSetLayout::PERSIST_STRIDE, 64u);   // AgentPersistState
    EXPECT_EQ(BufferSetLayout::OUTPUT_STRIDE, 64u);     // BehaviorOutput
    EXPECT_EQ(BufferSetLayout::DEBUG_STRIDE, 16u);
    EXPECT_EQ(BufferSetLayout::SPATIAL_CELL_STRIDE, 16u);
}
