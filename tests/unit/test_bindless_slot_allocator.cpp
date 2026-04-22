#include "vulkan/bindless_slot_allocator.h"

#include <gtest/gtest.h>
#include <unordered_set>

using namespace odyssey::vulkan;

// ---------------------------------------------------------------------------
// TextureHandle encoding tests
// ---------------------------------------------------------------------------

TEST(TextureHandle, DefaultIsInvalid) {
    TextureHandle h;
    EXPECT_FALSE(h.is_valid());
    EXPECT_EQ(h.raw, TextureHandle::INVALID);
}

TEST(TextureHandle, MakeRoundTrip) {
    // make() must recover slot and generation faithfully.
    auto h = TextureHandle::make(42u, 7u);
    EXPECT_TRUE(h.is_valid());
    EXPECT_EQ(h.slot(), 42u);
    EXPECT_EQ(h.generation(), 7u);
}

TEST(TextureHandle, Slot0IsSentinel) {
    auto h = TextureHandle::make(0u, 0u);
    EXPECT_TRUE(h.is_sentinel());
}

TEST(TextureHandle, MaxSlotAndGeneration) {
    // 16-bit fields — check boundary values.
    auto h = TextureHandle::make(0xFFFFu, 0xFFFFu);
    EXPECT_EQ(h.slot(), 0xFFFFu);
    EXPECT_EQ(h.generation(), 0xFFFFu);
}

// ---------------------------------------------------------------------------
// Allocator: happy-path tests
// ---------------------------------------------------------------------------

TEST(BindlessSlotAllocator, AllocFromEmpty) {
    BindlessSlotAllocator alloc(8);
    // free_count = capacity - 1 (slot 0 reserved).
    EXPECT_EQ(alloc.free_count(), 7u);

    auto result = alloc.alloc();
    ASSERT_TRUE(result.is_ok());
    TextureHandle h = result.value();
    EXPECT_TRUE(h.is_valid());
    // Slot 0 must never be handed out.
    EXPECT_NE(h.slot(), 0u);
    EXPECT_EQ(h.generation(), 0u); // fresh slot starts at generation 0
    EXPECT_EQ(alloc.free_count(), 6u);
}

TEST(BindlessSlotAllocator, AllocAllSlots) {
    const uint32_t cap = 4; // slots 0..3; slot 0 reserved → 3 allocatable
    BindlessSlotAllocator alloc(cap);

    std::unordered_set<uint32_t> seen;
    for (uint32_t i = 0; i < cap - 1; ++i) {
        auto r = alloc.alloc();
        ASSERT_TRUE(r.is_ok()) << "alloc #" << i << " failed";
        EXPECT_TRUE(seen.insert(r.value().slot()).second) << "duplicate slot";
        EXPECT_NE(r.value().slot(), 0u);
    }
    EXPECT_EQ(alloc.free_count(), 0u);
}

TEST(BindlessSlotAllocator, FreeThenRealloc) {
    BindlessSlotAllocator alloc(4);

    auto r1 = alloc.alloc();
    ASSERT_TRUE(r1.is_ok());
    TextureHandle h1 = r1.value();

    // Free h1.
    auto fr = alloc.free(h1);
    ASSERT_TRUE(fr.is_ok());

    // Re-alloc the same slot — new handle must have incremented generation.
    auto r2 = alloc.alloc();
    ASSERT_TRUE(r2.is_ok());
    TextureHandle h2 = r2.value();
    EXPECT_EQ(h2.slot(), h1.slot());           // same physical slot recycled
    EXPECT_NE(h2.generation(), h1.generation()); // stale handle detectable
    EXPECT_EQ(h2.generation(), (h1.generation() + 1u) & 0xFFFFu);
}

TEST(BindlessSlotAllocator, Slot0IsNeverAllocated) {
    BindlessSlotAllocator alloc(16);
    // Drain the whole free list.
    for (uint32_t i = 0; i < 15; ++i) {
        auto r = alloc.alloc();
        ASSERT_TRUE(r.is_ok());
        EXPECT_NE(r.value().slot(), 0u);
    }
}

TEST(BindlessSlotAllocator, OccupiedQueryCorrect) {
    BindlessSlotAllocator alloc(4);
    // Slot 0 always occupied.
    EXPECT_TRUE(alloc.is_occupied(0));

    auto r = alloc.alloc();
    ASSERT_TRUE(r.is_ok());
    uint32_t s = r.value().slot();
    EXPECT_TRUE(alloc.is_occupied(s));

    alloc.free(r.value());
    EXPECT_FALSE(alloc.is_occupied(s));
}

TEST(BindlessSlotAllocator, GenerationQueryMatchesHandle) {
    BindlessSlotAllocator alloc(4);
    auto r = alloc.alloc();
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(alloc.generation_of(r.value().slot()), r.value().generation());
}

// ---------------------------------------------------------------------------
// Allocator: failure-path tests (M2 mandate — one test per error mode)
// ---------------------------------------------------------------------------

TEST(BindlessSlotAllocator, AllocFromFullReturnsTableFull) {
    BindlessSlotAllocator alloc(2); // capacity=2 → 1 allocatable slot
    auto r1 = alloc.alloc();
    ASSERT_TRUE(r1.is_ok());

    auto r2 = alloc.alloc(); // table full
    ASSERT_TRUE(r2.is_err());
    EXPECT_EQ(r2.error(), AllocErr::TableFull);
}

TEST(BindlessSlotAllocator, DoubleFreeReturnsDoubleFree) {
    BindlessSlotAllocator alloc(4);
    auto r = alloc.alloc();
    ASSERT_TRUE(r.is_ok());
    TextureHandle h = r.value();

    auto fr1 = alloc.free(h);
    ASSERT_TRUE(fr1.is_ok());

    auto fr2 = alloc.free(h); // double-free
    ASSERT_TRUE(fr2.is_err());
    EXPECT_EQ(fr2.error(), AllocErr::DoubleFree);
}

TEST(BindlessSlotAllocator, FreeSlot0ReturnsOutOfRange) {
    BindlessSlotAllocator alloc(4);
    TextureHandle sentinel = TextureHandle::make(0u, 0u);

    auto fr = alloc.free(sentinel);
    ASSERT_TRUE(fr.is_err());
    EXPECT_EQ(fr.error(), AllocErr::OutOfRange);
}

TEST(BindlessSlotAllocator, FreeOutOfRangeSlotReturnsOutOfRange) {
    BindlessSlotAllocator alloc(4); // capacity=4, valid slots 0..3
    TextureHandle bad = TextureHandle::make(99u, 0u);

    auto fr = alloc.free(bad);
    ASSERT_TRUE(fr.is_err());
    EXPECT_EQ(fr.error(), AllocErr::OutOfRange);
}

TEST(BindlessSlotAllocator, StaleHandleReturnsGenerationMismatch) {
    // alloc a slot, free it, alloc the slot again (generation bumped),
    // then try to free the original (stale) handle.
    BindlessSlotAllocator alloc(4);

    auto r1 = alloc.alloc();
    ASSERT_TRUE(r1.is_ok());
    TextureHandle old_handle = r1.value();

    alloc.free(old_handle); // generation bumped

    auto r2 = alloc.alloc(); // same slot, new generation
    ASSERT_TRUE(r2.is_ok());
    EXPECT_EQ(r2.value().slot(), old_handle.slot());

    // Old handle now has wrong generation.
    auto fr = alloc.free(old_handle);
    ASSERT_TRUE(fr.is_err());
    EXPECT_EQ(fr.error(), AllocErr::GenerationMismatch);
}

// ---------------------------------------------------------------------------
// Allocator: capacity boundary
// ---------------------------------------------------------------------------

TEST(BindlessSlotAllocator, CapacityClampedToSlotMask) {
    // Requesting more than 65535 slots should clamp to the 16-bit limit.
    BindlessSlotAllocator alloc(100000u);
    EXPECT_EQ(alloc.capacity(), TextureHandle::SLOT_MASK);
}

// ---------------------------------------------------------------------------
// alloc_err_to_string helper
// ---------------------------------------------------------------------------

TEST(AllocErrToString, CoversAllCodes) {
    EXPECT_NE(alloc_err_to_string(AllocErr::TableFull), "");
    EXPECT_NE(alloc_err_to_string(AllocErr::DoubleFree), "");
    EXPECT_NE(alloc_err_to_string(AllocErr::OutOfRange), "");
    EXPECT_NE(alloc_err_to_string(AllocErr::GenerationMismatch), "");
}
