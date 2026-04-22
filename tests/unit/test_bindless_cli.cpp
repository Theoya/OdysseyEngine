#include "cli/cli.h"

#include <gtest/gtest.h>

using namespace odyssey::cli::commands;

// ---------------------------------------------------------------------------
// cmd_assets_bindless_stats — pure function (no GPU)
// ---------------------------------------------------------------------------

TEST(CmdAssetsBindlessStats, ZeroOccupancy) {
    auto result = cmd_assets_bindless_stats(0u, 16384u);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_FALSE(result.output.empty());
    // Output must mention capacity.
    EXPECT_NE(result.output.find("16384"), std::string::npos);
}

TEST(CmdAssetsBindlessStats, PartialOccupancy) {
    // 100 slots used out of 16384.
    auto result = cmd_assets_bindless_stats(100u, 16384u);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.output.find("100"), std::string::npos);
    EXPECT_NE(result.output.find("16284"), std::string::npos); // 16384 - 100
}

TEST(CmdAssetsBindlessStats, FullOccupancy) {
    auto result = cmd_assets_bindless_stats(16384u, 16384u);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.output.find("0"), std::string::npos); // 0 free
}

// ---------------------------------------------------------------------------
// cmd_assets_texture_count — pure function
// ---------------------------------------------------------------------------

TEST(CmdAssetsTextureCount, ZeroUsed) {
    // used=0 → authoring count = 0 (no sentinel to subtract from).
    auto result = cmd_assets_texture_count(0u);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.output.find("0"), std::string::npos);
}

TEST(CmdAssetsTextureCount, Onlysentinel) {
    // used=1 (sentinel only) → authoring count = 0.
    auto result = cmd_assets_texture_count(1u);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.output.find("0"), std::string::npos);
}

TEST(CmdAssetsTextureCount, TenTextures) {
    // used=11 (sentinel + 10 textures) → authoring count = 10.
    auto result = cmd_assets_texture_count(11u);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.output.find("10"), std::string::npos);
}
