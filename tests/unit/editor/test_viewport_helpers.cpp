// ---------------------------------------------------------------------------
// test_viewport_helpers.cpp
// Phase 2: pure helpers for the editor viewport panel.
// ---------------------------------------------------------------------------

#include "editor/viewport_panel.h"
#include "editor/scene_viewport_renderer.h"

#include <gtest/gtest.h>

using namespace odyssey;
using namespace odyssey::editor;

// ---------------------------------------------------------------------------
// compute_viewport_pixel_extent
// ---------------------------------------------------------------------------

TEST(ViewportPixelExtent, TypicalContentRegionRoundsDown) {
    auto e = compute_viewport_pixel_extent(1280.5f, 720.25f);
    EXPECT_EQ(e.width, 1280u);
    EXPECT_EQ(e.height, 720u);
}

TEST(ViewportPixelExtent, ClampsSmallInputsToMinimum) {
    auto e = compute_viewport_pixel_extent(1.0f, 1.0f);
    EXPECT_GE(e.width, 64u);
    EXPECT_GE(e.height, 64u);
}

TEST(ViewportPixelExtent, ClampsNegativeOrZeroToMinimum) {
    auto a = compute_viewport_pixel_extent(0.0f, 0.0f);
    EXPECT_GE(a.width, 64u);
    EXPECT_GE(a.height, 64u);
    auto b = compute_viewport_pixel_extent(-10.0f, -5.0f);
    EXPECT_GE(b.width, 64u);
    EXPECT_GE(b.height, 64u);
}

TEST(ViewportPixelExtent, LargeInputsPassThroughUntruncated) {
    auto e = compute_viewport_pixel_extent(3840.0f, 2160.0f);
    EXPECT_EQ(e.width, 3840u);
    EXPECT_EQ(e.height, 2160u);
}

// ---------------------------------------------------------------------------
// SceneViewportRenderer::final_layout — compile-time barrier assertion.
//
// The /barrier-audit contract demands the offscreen image be left in
// SHADER_READ_ONLY_OPTIMAL when handed to ImGui. This test pins that at
// compile-time so a future refactor cannot silently regress it.
// ---------------------------------------------------------------------------

TEST(ViewportBarrier, OffscreenFinalLayoutIsShaderReadOnly) {
    static_assert(SceneViewportRenderer::final_layout() ==
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  "Editor viewport renderer must leave offscreen image in "
                  "SHADER_READ_ONLY_OPTIMAL so ImGui can sample it directly.");
    EXPECT_EQ(SceneViewportRenderer::final_layout(),
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
