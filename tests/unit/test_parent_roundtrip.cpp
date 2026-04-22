#include <gtest/gtest.h>
#include <sstream>
#include "scene/scene_loader.h"
#include "scene/entity_manager.h"

using namespace odyssey;
using namespace odyssey::scene;

// Test: parent-less scenes round-trip byte-identically (no spurious parent="" added)
TEST(ParentRoundtrip, NoParentOmittedFromOutput) {
    // Minimal parent-less scene
    std::string xml = R"(
<scene name="test" version="1">
  <entity id="root" archetype="static">
    <transform position="0 0 0"/>
  </entity>
</scene>
)";

    auto result = parse_scene_xml(xml);
    ASSERT_TRUE(result.is_ok());

    auto scene = std::move(result).value();
    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_TRUE(scene.entities[0].parent_id.empty());
}

// Test: Scene with explicit parent reference parses correctly
TEST(ParentRoundtrip, ParentReferenceParses) {
    std::string xml = R"(
<scene name="test" version="1">
  <entity id="parent" archetype="static">
    <transform position="0 0 0"/>
  </entity>
  <entity id="child" archetype="static" parent="parent">
    <transform position="1 0 0"/>
  </entity>
</scene>
)";

    auto result = parse_scene_xml(xml);
    ASSERT_TRUE(result.is_ok());

    auto scene = std::move(result).value();
    ASSERT_EQ(scene.entities.size(), 2);
    EXPECT_TRUE(scene.entities[0].parent_id.empty());
    EXPECT_EQ(scene.entities[1].parent_id, "parent");
}

// Test: populate_entities resolves parent references by name
TEST(ParentRoundtrip, PopulateResolvesParent) {
    std::string xml = R"(
<scene name="test" version="1">
  <entity id="parent" archetype="static">
    <transform position="0 0 0"/>
  </entity>
  <entity id="child" archetype="static" parent="parent">
    <transform position="1 0 0"/>
  </entity>
</scene>
)";

    auto parse_result = parse_scene_xml(xml);
    ASSERT_TRUE(parse_result.is_ok());

    auto scene = std::move(parse_result).value();
    EntityManager manager;
    populate_entities(manager, scene);

    // Verify parent_id is set
    Entity* parent = manager.find_entity("parent");
    Entity* child = manager.find_entity("child");

    ASSERT_NE(parent, nullptr);
    ASSERT_NE(child, nullptr);

    EXPECT_EQ(child->parent_id, parent->id);
    // Parent should be top-level (INVALID_ENTITY)
    EXPECT_EQ(parent->parent_id, INVALID_ENTITY);
}

// Test: Invalid parent reference logs warning but doesn't crash
TEST(ParentRoundtrip, InvalidParentLogsWarning) {
    std::string xml = R"(
<scene name="test" version="1">
  <entity id="orphan" archetype="static" parent="nonexistent">
    <transform position="0 0 0"/>
  </entity>
</scene>
)";

    auto parse_result = parse_scene_xml(xml);
    ASSERT_TRUE(parse_result.is_ok());

    auto scene = std::move(parse_result).value();
    EntityManager manager;
    // Should not throw; logs warning but continues
    populate_entities(manager, scene);

    Entity* orphan = manager.find_entity("orphan");
    ASSERT_NE(orphan, nullptr);
    // parent_id remains unset (INVALID_ENTITY) due to invalid reference
    EXPECT_EQ(orphan->parent_id, INVALID_ENTITY);
}
