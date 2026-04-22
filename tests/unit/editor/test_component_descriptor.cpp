#include "editor/component_descriptor.h"
#include "scene/entity_manager.h"
#include <gtest/gtest.h>

using namespace odyssey::editor;
using namespace odyssey::scene;

class ComponentDescriptorTest : public ::testing::Test {
protected:
    Entity empty_entity{};
};

// F25: all_component_descriptors returns 8 entries in canonical order.
TEST_F(ComponentDescriptorTest, AllDescriptorsReturns8Entries) {
    const auto& descs = all_component_descriptors();
    EXPECT_EQ(descs.size(), 8u);
}

// F25: Component order is Transform, Stats, MeshRenderer, Behavior, Script,
// Tags, VoiceSource, PrefabSource.
TEST_F(ComponentDescriptorTest, ComponentOrderIsCanonical) {
    const auto& descs = all_component_descriptors();
    EXPECT_EQ(descs[0].kind, ComponentKind::Transform);
    EXPECT_EQ(descs[1].kind, ComponentKind::Stats);
    EXPECT_EQ(descs[2].kind, ComponentKind::MeshRenderer);
    EXPECT_EQ(descs[3].kind, ComponentKind::Behavior);
    EXPECT_EQ(descs[4].kind, ComponentKind::Script);
    EXPECT_EQ(descs[5].kind, ComponentKind::Tags);
    EXPECT_EQ(descs[6].kind, ComponentKind::VoiceSource);
    EXPECT_EQ(descs[7].kind, ComponentKind::PrefabSource);
}

// F25: missing_components on entity with default values.
TEST_F(ComponentDescriptorTest, MissingComponentsExcludesTransform) {
    auto missing = missing_components(empty_entity);
    // Empty entity has default stats and voice_range=25 (non-zero),
    // so Stats and VoiceSource are considered present.
    // Missing = all except Transform, Stats, and VoiceSource.
    EXPECT_EQ(missing.size(), 5u);  // All except Transform, Stats, VoiceSource
    for (const auto& kind : missing) {
        EXPECT_NE(kind, ComponentKind::Transform);
        EXPECT_NE(kind, ComponentKind::Stats);
        EXPECT_NE(kind, ComponentKind::VoiceSource);
    }
}

// F25: is_present predicates match field state.
TEST_F(ComponentDescriptorTest, IsPresentPredicatesDetectComponents) {
    const auto& descs = all_component_descriptors();

    // Transform is always present.
    EXPECT_TRUE(descs[0].is_present(empty_entity));

    // Stats is present by default (health=100, max_health=100, speed=5).
    EXPECT_TRUE(descs[1].is_present(empty_entity));

    // Mesh Renderer is absent when both paths are empty.
    EXPECT_FALSE(descs[2].is_present(empty_entity));

    // Add mesh path and check.
    empty_entity.components.mesh_path = "demo/mesh.mesh.xml";
    EXPECT_TRUE(descs[2].is_present(empty_entity));
}

// F25: add + remove round-trip on Mesh Renderer.
TEST_F(ComponentDescriptorTest, AddRemoveRoundTripStats) {
    const auto& descs = all_component_descriptors();
    auto& mesh_desc = descs[2];  // MeshRenderer

    // Initially absent.
    EXPECT_FALSE(mesh_desc.is_present(empty_entity));

    // Add.
    mesh_desc.add(empty_entity);
    EXPECT_TRUE(mesh_desc.is_present(empty_entity));

    // Remove.
    mesh_desc.remove(empty_entity);
    EXPECT_FALSE(mesh_desc.is_present(empty_entity));
}

// F25: Transform.removable is false.
TEST_F(ComponentDescriptorTest, TransformNotRemovable) {
    const auto& descs = all_component_descriptors();
    EXPECT_FALSE(descs[0].removable);
}

// F25: PrefabSource.removable is false.
TEST_F(ComponentDescriptorTest, PrefabSourceNotRemovable) {
    const auto& descs = all_component_descriptors();
    auto prefab_desc = std::find_if(descs.begin(), descs.end(),
        [](const ComponentDescriptor& d) {
            return d.kind == ComponentKind::PrefabSource;
        });
    EXPECT_NE(prefab_desc, descs.end());
    EXPECT_FALSE(prefab_desc->removable);
}

// F25: All other components are removable.
TEST_F(ComponentDescriptorTest, AllOtherComponentsRemovable) {
    const auto& descs = all_component_descriptors();
    for (size_t i = 1; i < descs.size() - 1; ++i) {  // Skip Transform (0) and PrefabSource (7)
        EXPECT_TRUE(descs[i].removable)
            << "Component at index " << i << " should be removable";
    }
}
