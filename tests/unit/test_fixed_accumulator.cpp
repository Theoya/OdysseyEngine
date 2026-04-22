#include <gtest/gtest.h>
#include "physics/physics_world.h"
#include "core/types.h"

using namespace odyssey;
using namespace odyssey::physics;

class FixedAccumulatorTest : public ::testing::Test {
protected:
    // Simulate the fixed-dt accumulator logic from Engine::process_frame
    struct AccumulatorState {
        double accumulator = 0.0;
        uint32_t total_substeps = 0;
        static constexpr float kFixedDt = 1.0f / 60.0f;
        static constexpr uint32_t kMaxSubsteps = 5;

        uint32_t step(float delta_time) {
            accumulator += delta_time;
            uint32_t substeps = 0;

            while (accumulator >= kFixedDt && substeps < kMaxSubsteps) {
                // In the real engine, physics_world.step(kFixedDt) happens here
                accumulator -= kFixedDt;
                ++substeps;
            }

            total_substeps += substeps;
            return substeps;
        }
    };
};

// Test: at frame dt = 1/60 (exact match), accumulator fires exactly 1 substep
TEST_F(FixedAccumulatorTest, ExactFixedFrameRate) {
    AccumulatorState state;
    constexpr float kFixedDt = 1.0f / 60.0f;

    uint32_t substeps = state.step(kFixedDt);
    EXPECT_EQ(substeps, 1);
    EXPECT_NEAR(state.accumulator, 0.0, 1e-6);
}

// Test: at frame dt = 1/30 (half rate), accumulator fires 2 substeps per frame
TEST_F(FixedAccumulatorTest, HalfFrameRateFires2Substeps) {
    AccumulatorState state;
    constexpr float kFixedDt = 1.0f / 60.0f;

    uint32_t substeps = state.step(2.0f * kFixedDt);
    EXPECT_EQ(substeps, 2);
    EXPECT_NEAR(state.accumulator, 0.0, 1e-6);
}

// Test: leftover time is preserved in accumulator
TEST_F(FixedAccumulatorTest, LeftoverTimePreserved) {
    AccumulatorState state;
    constexpr float kFixedDt = 1.0f / 60.0f;

    // Frame dt = 1/60 + 5ms extra
    float dt = kFixedDt + 0.005f;
    uint32_t substeps = state.step(dt);

    EXPECT_EQ(substeps, 1);
    EXPECT_NEAR(state.accumulator, 0.005f, 1e-6);
}

// Test: max 5 substeps is enforced even with large dt
TEST_F(FixedAccumulatorTest, MaxSubstepsEnforced) {
    AccumulatorState state;
    constexpr float kFixedDt = 1.0f / 60.0f;

    // Frame dt = 0.5 seconds (30 fixed steps worth)
    float dt = 0.5f;
    uint32_t substeps = state.step(dt);

    EXPECT_EQ(substeps, 5);  // capped at max
    // Leftover should be: 0.5 - (5 * 1/60) = 0.5 - 0.08333... = 0.41666...
    EXPECT_NEAR(state.accumulator, 0.5f - 5.0f * kFixedDt, 1e-6);
}

// Test: multiple frames build up accumulator correctly
TEST_F(FixedAccumulatorTest, MultiFrameAccumulation) {
    AccumulatorState state;
    constexpr float kFixedDt = 1.0f / 60.0f;

    // 10 frames at 1/90 fps (each frame is 1/90 seconds)
    float frame_dt = 1.0f / 90.0f;
    uint32_t total = 0;

    for (int i = 0; i < 10; ++i) {
        total += state.step(frame_dt);
    }

    // 10 frames * (1/90) = 10/90 = 1/9 ≈ 0.1111 seconds
    // In fixed 60 Hz, that's (0.1111 / (1/60)) ≈ 6.67 fixed steps
    // So we should get 6 fixed steps total, with ~0.01111 leftover
    EXPECT_EQ(total, 6);
    EXPECT_NEAR(state.accumulator, 10.0f * frame_dt - 6.0f * kFixedDt, 1e-6);
}
