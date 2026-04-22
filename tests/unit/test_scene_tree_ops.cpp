#include <gtest/gtest.h>
#include "editor/scene_tree_ops.h"
#include "scene/entity_manager.h"
#include "core/types.h"

using odyssey::EntityID;
using odyssey::scene::EntityManager;
using odyssey::scene::Entity;
using odyssey::editor::matches_filter;
using odyssey::editor::duplicate_entity;
using odyssey::editor::delete_entity;

class SceneTreeOpsTest : public ::testing::Test {
protected:
    EntityManager em_;
};

// ---- matches_filter tests ----

TEST_F(SceneTreeOpsTest, MatchesFilterEmptyFilter) {
    Entity e;
    e.name = "player";
    e.archetype = "player_character";

    // Empty filter matches everything
    EXPECT_TRUE(matches_filter(e, ""));
}

TEST_F(SceneTreeOpsTest, MatchesFilterByNameSubstring) {
    Entity e;
    e.name = "enemy_goblin_01";
    e.archetype = "enemy_goblin";

    EXPECT_TRUE(matches_filter(e, "goblin"));
    EXPECT_TRUE(matches_filter(e, "enemy"));
    EXPECT_FALSE(matches_filter(e, "orc"));
}

TEST_F(SceneTreeOpsTest, MatchesFilterCaseInsensitive) {
    Entity e;
    e.name = "PlayerCharacter";
    e.archetype = "player_class";

    EXPECT_TRUE(matches_filter(e, "player"));
    EXPECT_TRUE(matches_filter(e, "PLAYER"));
    EXPECT_TRUE(matches_filter(e, "PlAyEr"));
}

TEST_F(SceneTreeOpsTest, MatchesFilterByArchetype) {
    Entity e;
    e.name = "obj_123";
    e.archetype = "light_directional";

    EXPECT_TRUE(matches_filter(e, "light"));
    EXPECT_TRUE(matches_filter(e, "directional"));
}

// ---- duplicate_entity tests ----

TEST_F(SceneTreeOpsTest, DuplicateEntitySuccess) {
    EntityID orig = em_.create_entity("original", "test_archetype");
    Entity* orig_e = em_.get_entity(orig);
    ASSERT_NE(orig_e, nullptr);
    orig_e->components.mesh_path = "mesh.obj";

    auto dup_res = duplicate_entity(em_, orig);
    ASSERT_TRUE(dup_res.is_ok());
    EntityID dup_id = dup_res.value();
    EXPECT_NE(dup_id, orig);

    const Entity* dup_e = em_.get_entity(dup_id);
    ASSERT_NE(dup_e, nullptr);
    EXPECT_EQ(dup_e->name, "original (Copy)");
    EXPECT_EQ(dup_e->archetype, "test_archetype");
    EXPECT_EQ(dup_e->components.mesh_path, "mesh.obj");
}

TEST_F(SceneTreeOpsTest, DuplicateInvalidEntityReturnsErr) {
    auto res = duplicate_entity(em_, 9999);
    ASSERT_TRUE(res.is_err());
    EXPECT_NE(res.error().find("not found"), std::string::npos);
}

TEST_F(SceneTreeOpsTest, DuplicatePreservesComponents) {
    EntityID orig = em_.create_entity("src", "test_type");
    Entity* orig_e = em_.get_entity(orig);
    orig_e->components.behavior_shader = "behavior.nadir";
    orig_e->components.script_class = "MyScript";
    orig_e->components.voice_range = 50.0f;
    orig_e->active = false;

    auto dup_res = duplicate_entity(em_, orig);
    ASSERT_TRUE(dup_res.is_ok());
    const Entity* dup_e = em_.get_entity(dup_res.value());

    EXPECT_EQ(dup_e->components.behavior_shader, "behavior.nadir");
    EXPECT_EQ(dup_e->components.script_class, "MyScript");
    EXPECT_EQ(dup_e->components.voice_range, 50.0f);
    EXPECT_FALSE(dup_e->active);
}

// ---- delete_entity tests ----

TEST_F(SceneTreeOpsTest, DeleteEntitySuccess) {
    EntityID id = em_.create_entity("to_delete", "test");
    EXPECT_NE(em_.get_entity(id), nullptr);

    auto res = delete_entity(em_, id);
    ASSERT_TRUE(res.is_ok());
    EXPECT_EQ(em_.get_entity(id), nullptr);
}

TEST_F(SceneTreeOpsTest, DeleteInvalidEntityReturnsErr) {
    auto res = delete_entity(em_, 9999);
    ASSERT_TRUE(res.is_err());
    EXPECT_NE(res.error().find("not found"), std::string::npos);
}

TEST_F(SceneTreeOpsTest, DeleteRemovesFromEntityManager) {
    EntityID id1 = em_.create_entity("entity1", "type");
    EntityID id2 = em_.create_entity("entity2", "type");
    EXPECT_EQ(em_.entity_count(), 2);

    delete_entity(em_, id1);
    EXPECT_EQ(em_.entity_count(), 1);
    EXPECT_EQ(em_.get_entity(id1), nullptr);
    EXPECT_NE(em_.get_entity(id2), nullptr);
}
