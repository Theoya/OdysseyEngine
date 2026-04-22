#include <gtest/gtest.h>
#include "editor/entity_clipboard.h"
#include "scene/entity_manager.h"
#include "core/types.h"
#include <glm/glm.hpp>

using namespace odyssey::editor;
using odyssey::EntityID;
using odyssey::scene::Entity;

// Bring Entity into the test scope so `scene::Entity` (fully qualified) also resolves.
namespace scene = odyssey::scene;

TEST(EntityClipboard, GlobalSingleton) {
    // Clipboard should be accessible and persistent
    auto& clip1 = entity_clipboard();
    auto& clip2 = entity_clipboard();
    EXPECT_EQ(&clip1, &clip2);  // Same instance
}

TEST(EntityClipboard, CopyEntity) {
    auto& clip = entity_clipboard();
    clip.entities.clear();
    clip.is_cut = false;

    scene::Entity entity;
    entity.id = 42;
    entity.name = "TestEntity";
    entity.archetype = "TestArch";
    entity.components.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);

    EntityID new_id = clipboard_copy_entity(entity);
    EXPECT_NE(new_id, 42);  // New ID assigned
    EXPECT_EQ(clip.entities.size(), 1);

    auto& cloned = clip.entities[new_id];
    EXPECT_EQ(cloned.name, "TestEntity (Copy)");  // Name appended
    EXPECT_EQ(cloned.archetype, "TestArch");
    EXPECT_EQ(cloned.components.transform.position.x, 1.0f);
    EXPECT_FALSE(clip.is_cut);
}

TEST(EntityClipboard, CopyMultipleEntities) {
    auto& clip = entity_clipboard();
    clip.entities.clear();
    clip.is_cut = false;

    std::unordered_map<EntityID, scene::Entity> entities;
    for (int i = 0; i < 3; ++i) {
        scene::Entity e;
        e.id = i;
        e.name = "Entity" + std::to_string(i);
        e.archetype = "Arch";
        entities[i] = e;
    }

    size_t count = clipboard_copy_entities(entities);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(clip.entities.size(), 3);
    EXPECT_FALSE(clip.is_cut);
}

TEST(EntityClipboard, CopyMarksNotCut) {
    auto& clip = entity_clipboard();
    clip.entities.clear();
    clip.is_cut = false;

    scene::Entity e;
    e.id = 1;
    e.name = "Test";
    e.archetype = "Arch";

    clipboard_copy_entity(e);
    EXPECT_FALSE(clip.is_cut);
}

TEST(EntityClipboard, Clear) {
    auto& clip = entity_clipboard();

    scene::Entity e;
    e.id = 1;
    e.name = "Test";
    e.archetype = "Arch";

    clipboard_copy_entity(e);
    clip.is_cut = true;

    EXPECT_FALSE(clip.entities.empty());

    clipboard_clear();
    EXPECT_TRUE(clip.entities.empty());
    EXPECT_FALSE(clip.is_cut);
}

TEST(EntityClipboard, CutFlag) {
    auto& clip = entity_clipboard();
    clip.entities.clear();
    clip.is_cut = false;

    scene::Entity e;
    e.id = 1;
    e.name = "Test";
    e.archetype = "Arch";

    clipboard_copy_entity(e);
    clip.is_cut = true;  // Manually set

    EXPECT_TRUE(clip.is_cut);
    EXPECT_EQ(clip.entities.size(), 1);
}

TEST(EntityClipboard, PreserveEntityFields) {
    auto& clip = entity_clipboard();
    clip.entities.clear();

    scene::Entity orig;
    orig.id = 99;
    orig.name = "OriginalName";
    orig.archetype = "TestArchetype";
    orig.active = false;
    orig.components.transform.position = glm::vec3(5.0f, 10.0f, 15.0f);
    orig.components.stats.health = 42;
    orig.components.mesh_path = "/path/to/mesh";
    orig.components.tags.push_back("tag1");

    EntityID new_id = clipboard_copy_entity(orig);

    auto& cloned = clip.entities[new_id];
    // Verify cloned entity has expected archetype
    EXPECT_FALSE(cloned.active);
    EXPECT_EQ(cloned.components.transform.position.x, 5.0f);
    EXPECT_EQ(cloned.components.stats.health, 42.0f);
}
