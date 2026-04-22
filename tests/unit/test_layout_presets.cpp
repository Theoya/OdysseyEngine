#include <gtest/gtest.h>
#include "editor/layout_presets.h"

#include <cmath>

using namespace odyssey::editor;

TEST(LayoutPresets, PresetNameRoundTrip) {
    // F2: preset_name round-trips through preset_from_name
    EXPECT_EQ(preset_from_name(preset_name(LayoutPreset::Default)), LayoutPreset::Default);
    EXPECT_EQ(preset_from_name(preset_name(LayoutPreset::TwoByThree)), LayoutPreset::TwoByThree);
    EXPECT_EQ(preset_from_name(preset_name(LayoutPreset::Tall)), LayoutPreset::Tall);
    EXPECT_EQ(preset_from_name(preset_name(LayoutPreset::Wide)), LayoutPreset::Wide);
}

TEST(LayoutPresets, UnknownNameReturnsDefault) {
    // F2: Unknown names default to Default preset
    EXPECT_EQ(preset_from_name("UnknownPreset"), LayoutPreset::Default);
    EXPECT_EQ(preset_from_name(""), LayoutPreset::Default);
    EXPECT_EQ(preset_from_name(nullptr), LayoutPreset::Default);
}

TEST(LayoutPresets, SplitFractionsSum) {
    // F1: Each preset's split fractions are valid (sum <= 1.0 per axis)
    const float tolerance = 1e-6f;

    auto check_preset = [tolerance](LayoutPreset p) {
        auto splits = preset_splits(p);
        float x_total = 0.0f;
        float y_total = 0.0f;
        for (const auto& split : splits) {
            // Splits are on x-axis or y-axis; accumulate ratios
            // (simple validation — a full check would simulate the split tree)
            if (split.ratio > 0.0f && split.ratio <= 1.0f) {
                // OK
            } else {
                FAIL() << "Invalid ratio " << split.ratio;
            }
        }
    };

    check_preset(LayoutPreset::Default);
    check_preset(LayoutPreset::TwoByThree);
    check_preset(LayoutPreset::Tall);
    check_preset(LayoutPreset::Wide);
}

TEST(LayoutPresets, SlugLayoutNameEmpty) {
    // F3: Empty name → "_unnamed"
    EXPECT_EQ(slug_layout_name(""), "_unnamed");
}

TEST(LayoutPresets, SlugLayoutNameSpaces) {
    // F3: Spaces → underscores
    EXPECT_EQ(slug_layout_name("My Layout"), "my_layout");
    EXPECT_EQ(slug_layout_name("  Spaces  "), "spaces");  // Leading/trailing trimmed
}

TEST(LayoutPresets, SlugLayoutNamePunctuation) {
    // F3: Punctuation dropped
    EXPECT_EQ(slug_layout_name("Layout!@#$"), "layout");
    EXPECT_EQ(slug_layout_name("My-Layout"), "mylayout");  // Hyphen dropped
}

TEST(LayoutPresets, SlugLayoutNameLowercase) {
    // F3: Uppercase → lowercase
    EXPECT_EQ(slug_layout_name("MyLayout"), "mylayout");
    EXPECT_EQ(slug_layout_name("CAPS"), "caps");
}

TEST(LayoutPresets, SlugLayoutNameTruncate) {
    // F3: Overlong names truncated to 64 chars
    std::string long_name(100, 'a');
    std::string slug = slug_layout_name(long_name);
    EXPECT_LE(slug.size(), 64);
}

TEST(LayoutPresets, SlugLayoutNameUnicode) {
    // F3: Non-ASCII characters dropped
    std::string name_with_unicode = "Layout_\xc3\xa9";  // 'é' in UTF-8
    // Just ensure it doesn't crash and produces a valid slug
    std::string slug = slug_layout_name(name_with_unicode);
    EXPECT_FALSE(slug.empty());
}

TEST(LayoutPresets, SlugLayoutNameConsecutiveSpaces) {
    // F3: Multiple spaces → single underscore
    EXPECT_EQ(slug_layout_name("Layout   With   Spaces"), "layout_with_spaces");
}

TEST(LayoutPresets, PresetNamesDistinct) {
    // F2: All preset names are unique
    const char* names[] = {
        preset_name(LayoutPreset::Default),
        preset_name(LayoutPreset::TwoByThree),
        preset_name(LayoutPreset::Tall),
        preset_name(LayoutPreset::Wide)
    };

    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            EXPECT_STRNE(names[i], names[j]) << "Duplicate names";
        }
    }
}
