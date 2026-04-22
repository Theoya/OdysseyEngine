#include <gtest/gtest.h>
#include "audio/music/detail/leitmotif_table.h"

using odyssey::audio::music::detail::LeitmotifTable;
using odyssey::audio::music::detail::LeitmotifDef;

TEST(LeitmotifTableTest, InsertAndLookup) {
    LeitmotifTable table;
    LeitmotifDef motif;
    motif.id = 1;
    motif.source_clip = "clips/heroic_theme.wav";
    motif.emotional_register = "heroic";
    motif.permitted_contexts = "combat,exploration";
    motif.min_reentry_bars = 4;

    table.insert(motif);
    const auto* found = table.lookup(1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, 1);
    EXPECT_EQ(found->source_clip, "clips/heroic_theme.wav");
}

TEST(LeitmotifTableTest, LookupNotFound) {
    LeitmotifTable table;
    const auto* found = table.lookup(999);
    EXPECT_EQ(found, nullptr);
}

TEST(LeitmotifTableTest, O1Access) {
    LeitmotifTable table;
    // Insert many motifs
    for (uint32_t i = 0; i < 1000; ++i) {
        LeitmotifDef motif;
        motif.id = i;
        motif.source_clip = "clip_" + std::to_string(i) + ".wav";
        table.insert(motif);
    }
    // Lookup is O(1) regardless of table size
    const auto* found = table.lookup(500);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, 500);
}

TEST(LeitmotifTableTest, Clear) {
    LeitmotifTable table;
    LeitmotifDef motif;
    motif.id = 1;
    table.insert(motif);
    EXPECT_EQ(table.size(), 1);
    table.clear();
    EXPECT_EQ(table.size(), 0);
}
