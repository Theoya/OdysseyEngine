// ---------------------------------------------------------------------------
// test_status_bar.cpp
//
// Unit tests for src/editor/status_bar.{h,cpp}.
// Covers:
//   - compute_fps_ema: converges correctly
//   - compute_fps_ema: dt=0 preserves prev
//   - compute_fps_ema: negative alpha clamps to 0
// ---------------------------------------------------------------------------

#include "editor/status_bar.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace odyssey::editor;

// ---------------------------------------------------------------------------
// compute_fps_ema: convergence
// ---------------------------------------------------------------------------

TEST(StatusBar, EMAConverges) {
    // Start at 30 FPS, smoothly converge to 60 FPS.
    float ema = 30.0f;
    const float alpha = 0.1f;
    const float target = 60.0f;

    for (int i = 0; i < 100; i++) {
        ema = compute_fps_ema(ema, target, alpha);
    }

    // After 100 iterations with α=0.1, should be very close to 60.
    EXPECT_NEAR(ema, 60.0f, 0.1f);
}

// ---------------------------------------------------------------------------
// compute_fps_ema: zero dt
// ---------------------------------------------------------------------------

TEST(StatusBar, EMAPrevervesWhenNewValIsZero) {
    float ema = 45.0f;
    float new_val = 0.0f;
    float alpha = 0.5f;

    // new_ema = 0.5 * 0 + 0.5 * 45 = 22.5
    float result = compute_fps_ema(ema, new_val, alpha);
    EXPECT_FLOAT_EQ(result, 22.5f);
}

// ---------------------------------------------------------------------------
// compute_fps_ema: negative alpha clamps
// ---------------------------------------------------------------------------

TEST(StatusBar, EMANegativeAlphaClampsToZero) {
    float ema = 30.0f;
    float new_val = 60.0f;
    float alpha = -0.5f;

    // alpha should be clamped to 0.
    float result = compute_fps_ema(ema, new_val, alpha);

    // With alpha=0, result = 0 * 60 + 1 * 30 = 30 (no change).
    EXPECT_FLOAT_EQ(result, 30.0f);
}

TEST(StatusBar, EMAAlphaGreaterThanOneClampsToOne) {
    float ema = 30.0f;
    float new_val = 60.0f;
    float alpha = 1.5f;

    // alpha should be clamped to 1.
    float result = compute_fps_ema(ema, new_val, alpha);

    // With alpha=1, result = 1 * 60 + 0 * 30 = 60 (full new value).
    EXPECT_FLOAT_EQ(result, 60.0f);
}

// ---------------------------------------------------------------------------
// compute_fps_ema: normal case
// ---------------------------------------------------------------------------

TEST(StatusBar, EMAPropagatesAlpha) {
    float ema = 50.0f;
    float new_val = 100.0f;
    float alpha = 0.2f;

    // ema = 0.2 * 100 + 0.8 * 50 = 20 + 40 = 60.
    float result = compute_fps_ema(ema, new_val, alpha);
    EXPECT_FLOAT_EQ(result, 60.0f);
}
