#include <gtest/gtest.h>
#include "editor/play_snapshot.h"
#include "scene/entity_manager.h"
#include "scene/scene_loader.h"

using namespace odyssey;

TEST(PlaySnapshot, CaptureAndRestore) {
    // F1: Round-trip clone preserves scene_data and entity fields
    scene::SceneData original_data;
    original_data.name = "TestScene";

    scene::EntityManager em;
    auto e1_id = em.create_entity("Entity1", "TestArchetype");
    auto e2_id = em.create_entity("Entity2", "TestArchetype");

    auto* e1 = em.get_entity(e1_id);
    ASSERT_NE(e1, nullptr);
    e1->components.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    e1->components.stats.health = 100;

    // Capture snapshot
    auto snap_res = editor::capture_snapshot(original_data, em);
    ASSERT_TRUE(snap_res.is_ok());
    auto snap = snap_res.value();

    // Verify snapshot contains the entities
    EXPECT_EQ(snap.entities_snapshot.size(), 2);
    EXPECT_TRUE(snap.entities_snapshot.count(e1_id));
    EXPECT_TRUE(snap.entities_snapshot.count(e2_id));

    // Verify entity data is cloned
    EXPECT_EQ(snap.entities_snapshot[e1_id].components.transform.position.x, 1.0f);
    EXPECT_EQ(snap.entities_snapshot[e1_id].components.stats.health, 100);

    // Restore into new instances
    scene::SceneData restored_data;
    scene::EntityManager restored_em;

    auto restore_res = editor::restore_snapshot(snap, restored_data, restored_em);
    ASSERT_TRUE(restore_res.is_ok());

    // Verify restoration
    EXPECT_EQ(restored_em.entity_count(), 2);
    auto* r1 = restored_em.get_entity(e1_id);
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(r1->components.transform.position.x, 1.0f);
    EXPECT_EQ(r1->components.stats.health, 100);
}

TEST(PlaySnapshot, RestoreIntoMutatedManager) {
    // F2: Restore into a mutated EntityManager resets it
    scene::SceneData snap_data;
    scene::EntityManager snap_em;
    auto snap_e = snap_em.create_entity("SnapEntity", "Archetype");

    auto snap_res = editor::capture_snapshot(snap_data, snap_em);
    ASSERT_TRUE(snap_res.is_ok());
    auto snap = snap_res.value();

    // Create a different manager with different entities
    scene::EntityManager mutated;
    mutated.create_entity("Different1", "OtherArchetype");
    mutated.create_entity("Different2", "OtherArchetype");
    EXPECT_EQ(mutated.entity_count(), 2);

    // Restore should replace the mutated manager's state
    scene::SceneData restored;
    auto restore_res = editor::restore_snapshot(snap, restored, mutated);
    ASSERT_TRUE(restore_res.is_ok());

    // Should now match the snapshot
    EXPECT_EQ(mutated.entity_count(), 1);
    EXPECT_NE(mutated.get_entity(snap_e), nullptr);
}

TEST(PlaySnapshot, ClonedDataIsDeep) {
    // F3: Mutation of original doesn't affect snapshot
    scene::SceneData original;
    original.name = "Original";

    scene::EntityManager em;
    auto e_id = em.create_entity("Entity", "Archetype");
    auto* e = em.get_entity(e_id);
    e->components.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);

    auto snap_res = editor::capture_snapshot(original, em);
    ASSERT_TRUE(snap_res.is_ok());
    auto snap = snap_res.value();

    // Mutate original after snapshot
    original.name = "Modified";
    e->components.transform.position = glm::vec3(10.0f, 20.0f, 30.0f);

    // Snapshot should be unchanged
    EXPECT_EQ(snap.scene_data_snapshot.name, "Original");
    EXPECT_EQ(snap.entities_snapshot[e_id].components.transform.position.x, 1.0f);
}

TEST(PlaySnapshot, RoundTripPreservesAllFields) {
    // F4: Complete field preservation
    scene::SceneData data;
    data.name = "ComplexScene";

    scene::EntityManager em;
    auto e_id = em.create_entity("TestEntity", "TestArch");
    auto* e = em.get_entity(e_id);
    e->components.transform.position = glm::vec3(5.0f, 10.0f, 15.0f);
    e->components.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    e->components.stats.health = 75;
    e->components.stats.max_health = 100;
    e->components.mesh_path = "/path/to/mesh";
    e->components.voice_range = 30.0f;
    e->components.tags.push_back("tag1");
    e->components.tags.push_back("tag2");
    e->name = "CustomName";
    e->active = false;

    auto snap_res = editor::capture_snapshot(data, em);
    ASSERT_TRUE(snap_res.is_ok());
    auto snap = snap_res.value();

    // Restore and verify all fields
    scene::SceneData restored;
    scene::EntityManager restored_em;
    auto restore_res = editor::restore_snapshot(snap, restored, restored_em);
    ASSERT_TRUE(restore_res.is_ok());

    auto* r = restored_em.get_entity(e_id);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->components.transform.position.x, 5.0f);
    EXPECT_EQ(r->components.transform.position.y, 10.0f);
    EXPECT_EQ(r->components.stats.health, 75);
    EXPECT_EQ(r->components.mesh_path, "/path/to/mesh");
    EXPECT_EQ(r->components.voice_range, 30.0f);
    EXPECT_EQ(r->components.tags.size(), 2);
    EXPECT_EQ(r->name, "CustomName");
    EXPECT_FALSE(r->active);
}
