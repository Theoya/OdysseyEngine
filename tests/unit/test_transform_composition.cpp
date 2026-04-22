#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include "core/types.h"
#include "scene/entity_manager.h"

using namespace odyssey;
using namespace odyssey::scene;

class TransformCompositionTest : public ::testing::Test {
protected:
    EntityManager manager;

    void SetUp() override {
        manager.clear();
    }
};

// Case 1: Identity — single top-level entity, no parent
TEST_F(TransformCompositionTest, TopLevelEntityIdentity) {
    EntityID id = manager.create_entity("root", "test");
    Entity* e = manager.get_entity(id);
    ASSERT_NE(e, nullptr);

    e->components.transform.position = vec3{5.0f, 10.0f, 15.0f};
    e->components.transform.rotation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    e->components.transform.scale = vec3{2.0f, 2.0f, 2.0f};

    auto result = manager.compose_world_transforms();
    EXPECT_TRUE(result.is_ok());

    EXPECT_EQ(e->world_transform.position, e->components.transform.position);
    EXPECT_EQ(e->world_transform.rotation, e->components.transform.rotation);
    EXPECT_EQ(e->world_transform.scale, e->components.transform.scale);
}

// Case 2: Single parent — child's world = parent.world × child.local
TEST_F(TransformCompositionTest, SingleParentComposition) {
    EntityID parent_id = manager.create_entity("parent", "test");
    EntityID child_id = manager.create_entity("child", "test");

    Entity* parent = manager.get_entity(parent_id);
    Entity* child = manager.get_entity(child_id);

    ASSERT_NE(parent, nullptr);
    ASSERT_NE(child, nullptr);

    // Parent: position (10, 0, 0), scale 2x
    parent->components.transform.position = vec3{10.0f, 0.0f, 0.0f};
    parent->components.transform.rotation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    parent->components.transform.scale = vec3{2.0f, 2.0f, 2.0f};

    // Child: local position (5, 0, 0) (relative to parent)
    child->components.transform.position = vec3{5.0f, 0.0f, 0.0f};
    child->components.transform.rotation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    child->components.transform.scale = vec3{1.0f, 1.0f, 1.0f};

    child->parent_id = parent_id;

    auto result = manager.compose_world_transforms();
    EXPECT_TRUE(result.is_ok());

    // Child's world position should be parent.pos + (parent.scale * child.local.pos)
    // = (10, 0, 0) + 2 * (5, 0, 0) = (20, 0, 0)
    EXPECT_NEAR(child->world_transform.position.x, 20.0f, 0.001f);
    EXPECT_NEAR(child->world_transform.position.y, 0.0f, 0.001f);
    EXPECT_NEAR(child->world_transform.position.z, 0.0f, 0.001f);

    // Scale should compound: 2 * 1 = 2
    EXPECT_NEAR(child->world_transform.scale.x, 2.0f, 0.001f);
    EXPECT_NEAR(child->world_transform.scale.y, 2.0f, 0.001f);
    EXPECT_NEAR(child->world_transform.scale.z, 2.0f, 0.001f);
}

// Case 3: Nested 3-deep hierarchy
TEST_F(TransformCompositionTest, NestedHierarchy3Deep) {
    EntityID id1 = manager.create_entity("root", "test");
    EntityID id2 = manager.create_entity("child1", "test");
    EntityID id3 = manager.create_entity("child2", "test");

    Entity* e1 = manager.get_entity(id1);
    Entity* e2 = manager.get_entity(id2);
    Entity* e3 = manager.get_entity(id3);

    ASSERT_NE(e1, nullptr);
    ASSERT_NE(e2, nullptr);
    ASSERT_NE(e3, nullptr);

    // Root at origin
    e1->components.transform.position = vec3{0.0f, 0.0f, 0.0f};
    e1->components.transform.scale = vec3{1.0f, 1.0f, 1.0f};

    // Child1: local (1, 0, 0), parent=root
    e2->components.transform.position = vec3{1.0f, 0.0f, 0.0f};
    e2->components.transform.scale = vec3{1.0f, 1.0f, 1.0f};
    e2->parent_id = id1;

    // Child2: local (1, 0, 0), parent=child1
    e3->components.transform.position = vec3{1.0f, 0.0f, 0.0f};
    e3->components.transform.scale = vec3{1.0f, 1.0f, 1.0f};
    e3->parent_id = id2;

    auto result = manager.compose_world_transforms();
    EXPECT_TRUE(result.is_ok());

    // e1 world = (0, 0, 0)
    EXPECT_NEAR(e1->world_transform.position.x, 0.0f, 0.001f);

    // e2 world = (1, 0, 0)
    EXPECT_NEAR(e2->world_transform.position.x, 1.0f, 0.001f);

    // e3 world = e2.world + e3.local = (1, 0, 0) + (1, 0, 0) = (2, 0, 0)
    EXPECT_NEAR(e3->world_transform.position.x, 2.0f, 0.001f);
}

// Case 4: Cycle detection — self-parent
TEST_F(TransformCompositionTest, CycleDetectionSelfParent) {
    EntityID id = manager.create_entity("self_loop", "test");
    Entity* e = manager.get_entity(id);
    ASSERT_NE(e, nullptr);

    e->parent_id = id;  // Self-parent

    auto result = manager.compose_world_transforms();
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), HierarchyError::SelfParent);
}

// Case 5: Unknown parent validation
TEST_F(TransformCompositionTest, UnknownParentValidation) {
    EntityID id = manager.create_entity("orphan", "test");
    Entity* e = manager.get_entity(id);
    ASSERT_NE(e, nullptr);

    e->parent_id = 9999;  // Non-existent parent ID

    auto result = manager.compose_world_transforms();
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), HierarchyError::UnknownParent);
}

// Case 6: Cycle in chain A -> B -> C -> A
TEST_F(TransformCompositionTest, CycleDetectionChain) {
    EntityID id_a = manager.create_entity("a", "test");
    EntityID id_b = manager.create_entity("b", "test");
    EntityID id_c = manager.create_entity("c", "test");

    Entity* ea = manager.get_entity(id_a);
    Entity* eb = manager.get_entity(id_b);
    Entity* ec = manager.get_entity(id_c);

    ASSERT_NE(ea, nullptr);
    ASSERT_NE(eb, nullptr);
    ASSERT_NE(ec, nullptr);

    ea->parent_id = id_c;
    eb->parent_id = id_a;
    ec->parent_id = id_b;

    auto result = manager.compose_world_transforms();
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), HierarchyError::Cycle);
}

// Case 7: Depth 64 — at limit, should pass
TEST_F(TransformCompositionTest, Depth64OK) {
    EntityID prev_id = manager.create_entity("root", "test");
    Entity* prev = manager.get_entity(prev_id);
    ASSERT_NE(prev, nullptr);

    for (int i = 1; i < 64; ++i) {
        EntityID id = manager.create_entity("e" + std::to_string(i), "test");
        Entity* e = manager.get_entity(id);
        ASSERT_NE(e, nullptr);
        e->parent_id = prev_id;
        prev_id = id;
    }

    auto result = manager.compose_world_transforms();
    EXPECT_TRUE(result.is_ok());
}

// Case 8: Depth 65 — exceeds limit, should fail
TEST_F(TransformCompositionTest, Depth65Rejected) {
    EntityID prev_id = manager.create_entity("root", "test");
    Entity* prev = manager.get_entity(prev_id);
    ASSERT_NE(prev, nullptr);

    for (int i = 1; i <= 65; ++i) {
        EntityID id = manager.create_entity("e" + std::to_string(i), "test");
        Entity* e = manager.get_entity(id);
        ASSERT_NE(e, nullptr);
        e->parent_id = prev_id;
        prev_id = id;
    }

    auto result = manager.compose_world_transforms();
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), HierarchyError::DepthExceeded);
}
