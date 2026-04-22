#include <gtest/gtest.h>
#include "scene/scene_loader.h"
#include "core/types.h"

using namespace odyssey;
using namespace odyssey::scene;

// Test: Rigidbody component parses successfully with defaults
TEST(ComponentParsingTest, RigidbodyDefaults) {
    auto result = parse_rigidbody(pugi::xml_document().root());
    ASSERT_TRUE(result.is_ok());

    auto rb = std::move(result).value();
    EXPECT_EQ(rb.mass, 1.0f);
    EXPECT_EQ(rb.drag, 0.01f);
    EXPECT_EQ(rb.angular_drag, 0.05f);
    EXPECT_TRUE(rb.use_gravity);
    EXPECT_FALSE(rb.is_kinematic);
}

// Test: Rigidbody rejects zero mass
TEST(ComponentParsingTest, RigidbodyZeroMassRejected) {
    pugi::xml_document doc;
    doc.load_string("<rigidbody mass=\"0\"/>");
    auto result = parse_rigidbody(doc.root().first_child());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(std::string(result.error()).find("mass"), std::string::npos);
}

// Test: Rigidbody rejects negative drag
TEST(ComponentParsingTest, RigidbodyNegativeDragRejected) {
    pugi::xml_document doc;
    doc.load_string("<rigidbody mass=\"10\" drag=\"-0.5\"/>");
    auto result = parse_rigidbody(doc.root().first_child());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(std::string(result.error()).find("drag"), std::string::npos);
}

// Test: SphereCollider rejects zero radius
TEST(ComponentParsingTest, SphereColliderZeroRadiusRejected) {
    pugi::xml_document doc;
    doc.load_string("<sphere_collider radius=\"0\"/>");
    auto result = parse_sphere_collider(doc.root().first_child());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(std::string(result.error()).find("radius"), std::string::npos);
}

// Test: CapsuleCollider rejects negative radius
TEST(ComponentParsingTest, CapsuleColliderNegativeRadiusRejected) {
    pugi::xml_document doc;
    doc.load_string("<capsule_collider radius=\"-0.5\" height=\"2.0\"/>");
    auto result = parse_capsule_collider(doc.root().first_child());
    EXPECT_TRUE(result.is_err());
}

// Test: CapsuleCollider rejects height < 2*radius
TEST(ComponentParsingTest, CapsuleColliderHeightTooSmallRejected) {
    pugi::xml_document doc;
    doc.load_string("<capsule_collider radius=\"0.5\" height=\"0.5\"/>");
    auto result = parse_capsule_collider(doc.root().first_child());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(std::string(result.error()).find("height"), std::string::npos);
}

// Test: Camera rejects invalid FOV
TEST(ComponentParsingTest, CameraInvalidFovRejected) {
    pugi::xml_document doc;
    doc.load_string("<camera fov=\"200\"/>");
    auto result = parse_camera(doc.root().first_child());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(std::string(result.error()).find("fov"), std::string::npos);
}

// Test: Camera rejects far <= near
TEST(ComponentParsingTest, CameraFarNotGreaterThanNear) {
    pugi::xml_document doc;
    doc.load_string("<camera near=\"10\" far=\"5\"/>");
    auto result = parse_camera(doc.root().first_child());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(std::string(result.error()).find("far"), std::string::npos);
}

// Test: BoxCollider rejects zero half-extent
TEST(ComponentParsingTest, BoxColliderZeroHalfExtentRejected) {
    pugi::xml_document doc;
    doc.load_string("<box_collider half_extents=\"0.5 0 0.5\"/>");
    auto result = parse_box_collider(doc.root().first_child());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(std::string(result.error()).find("half_extents"), std::string::npos);
}
