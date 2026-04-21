// ---------------------------------------------------------------------------
// test_math_util.cpp
//
// Unit tests for src/core/math_util.h — pure math helpers used by the
// Inspector (euler<->quat) and future subsystems. All tests are value-in /
// value-out, no state, no allocator.
// ---------------------------------------------------------------------------

#include "core/math_util.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace odyssey;

static void expect_quat_near(const quat& a, const quat& b, float eps = 1e-4f) {
    // A quaternion and its negation represent the same rotation. Accept
    // either for equality.
    const float d_pos = (a.w - b.w) * (a.w - b.w) +
                        (a.x - b.x) * (a.x - b.x) +
                        (a.y - b.y) * (a.y - b.y) +
                        (a.z - b.z) * (a.z - b.z);
    const float d_neg = (a.w + b.w) * (a.w + b.w) +
                        (a.x + b.x) * (a.x + b.x) +
                        (a.y + b.y) * (a.y + b.y) +
                        (a.z + b.z) * (a.z + b.z);
    EXPECT_LT(std::min(d_pos, d_neg), eps) <<
        "a=(" << a.w << "," << a.x << "," << a.y << "," << a.z <<
        ") b=(" << b.w << "," << b.x << "," << b.y << "," << b.z << ")";
}

TEST(MathUtil, DegRadRoundTrip) {
    EXPECT_FLOAT_EQ(rad_to_deg(deg_to_rad(180.0f)), 180.0f);
    EXPECT_NEAR(deg_to_rad(180.0f), 3.14159265f, 1e-5f);
    EXPECT_NEAR(rad_to_deg(3.14159265f), 180.0f, 1e-3f);
}

TEST(MathUtil, ZeroEulerGivesIdentityQuat) {
    auto q = euler_xyz_deg_to_quat(vec3{0.0f, 0.0f, 0.0f});
    EXPECT_TRUE(quat_is_identity(q));
}

TEST(MathUtil, IdentityQuatGivesZeroEuler) {
    quat q{1.0f, 0.0f, 0.0f, 0.0f};
    auto e = quat_to_euler_xyz_deg(q);
    EXPECT_NEAR(e.x, 0.0f, 1e-3f);
    EXPECT_NEAR(e.y, 0.0f, 1e-3f);
    EXPECT_NEAR(e.z, 0.0f, 1e-3f);
}

TEST(MathUtil, RollOnlyRoundTrips) {
    // 45 degree roll (x-axis).
    vec3 e_in{45.0f, 0.0f, 0.0f};
    auto q = euler_xyz_deg_to_quat(e_in);
    auto e_out = quat_to_euler_xyz_deg(q);
    EXPECT_NEAR(e_out.x, 45.0f, 1e-3f);
    EXPECT_NEAR(e_out.y, 0.0f,  1e-3f);
    EXPECT_NEAR(e_out.z, 0.0f,  1e-3f);
}

TEST(MathUtil, YawOnlyRoundTrips) {
    vec3 e_in{0.0f, 0.0f, 30.0f};
    auto q = euler_xyz_deg_to_quat(e_in);
    auto e_out = quat_to_euler_xyz_deg(q);
    EXPECT_NEAR(e_out.x, 0.0f,  1e-3f);
    EXPECT_NEAR(e_out.y, 0.0f,  1e-3f);
    EXPECT_NEAR(e_out.z, 30.0f, 1e-3f);
}

TEST(MathUtil, PitchOnlyRoundTrips) {
    vec3 e_in{0.0f, 60.0f, 0.0f};
    auto q = euler_xyz_deg_to_quat(e_in);
    auto e_out = quat_to_euler_xyz_deg(q);
    EXPECT_NEAR(e_out.x, 0.0f,  1e-3f);
    EXPECT_NEAR(e_out.y, 60.0f, 1e-3f);
    EXPECT_NEAR(e_out.z, 0.0f,  1e-3f);
}

TEST(MathUtil, CombinedRotationRoundTrips) {
    // Stay well away from the pitch=90 singularity.
    vec3 e_in{15.0f, 35.0f, 55.0f};
    auto q = euler_xyz_deg_to_quat(e_in);
    auto e_out = quat_to_euler_xyz_deg(q);
    // Round-trip through the quaternion should preserve the same rotation.
    auto q2 = euler_xyz_deg_to_quat(e_out);
    expect_quat_near(q, q2);
}

TEST(MathUtil, DenormalQuatIsNormalizedInternally) {
    // A quaternion 5x scaled is still a rotation after internal normalize —
    // the output euler must match the same direction.
    quat q_unit = euler_xyz_deg_to_quat(vec3{10.0f, 20.0f, 30.0f});
    quat q_scaled{5.0f * q_unit.w, 5.0f * q_unit.x,
                  5.0f * q_unit.y, 5.0f * q_unit.z};
    auto e_a = quat_to_euler_xyz_deg(q_unit);
    auto e_b = quat_to_euler_xyz_deg(q_scaled);
    EXPECT_NEAR(e_a.x, e_b.x, 1e-3f);
    EXPECT_NEAR(e_a.y, e_b.y, 1e-3f);
    EXPECT_NEAR(e_a.z, e_b.z, 1e-3f);
}

TEST(MathUtil, IdentityPredicate) {
    EXPECT_TRUE(quat_is_identity(quat{1.0f, 0.0f, 0.0f, 0.0f}));
    EXPECT_FALSE(quat_is_identity(quat{0.707f, 0.707f, 0.0f, 0.0f}));
    // Near-identity within eps.
    EXPECT_TRUE(quat_is_identity(quat{1.0f - 1e-6f, 0.0f, 0.0f, 0.0f}));
}
