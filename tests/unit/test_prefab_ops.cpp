#include "editor/prefab_ops.h"
#include "scene/entity_manager.h"
#include "core/types.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

using namespace odyssey::editor;
using namespace odyssey::scene;

class PrefabOpsTest : public ::testing::Test {
protected:
    std::filesystem::path temp_dir_;

    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / "odyssey_prefab_test";
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
    }
};

// Test create_prefab_from_entity writes a valid XML file
TEST_F(PrefabOpsTest, CreatePrefabWritesValidXML) {
    EntityManager em;

    // Create a test entity
    odyssey::EntityID entity_id = em.create_entity("test_entity", "box");
    auto* entity = em.get_entity(entity_id);
    ASSERT_TRUE(entity != nullptr);

    // Create prefab
    auto res = create_prefab_from_entity(em, entity_id, temp_dir_);
    ASSERT_TRUE(res.is_ok());

    std::filesystem::path prefab_path = res.value();
    EXPECT_TRUE(std::filesystem::exists(prefab_path));

    // Verify file contains expected XML structure
    std::ifstream file(prefab_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("<prefab>"), std::string::npos);
    EXPECT_NE(content.find("<entity"), std::string::npos);
    EXPECT_NE(content.find("test_entity"), std::string::npos);
}

// Test create_prefab_from_entity with invalid EntityID returns Err
TEST_F(PrefabOpsTest, CreatePrefabInvalidEntityReturnsErr) {
    EntityManager em;

    // Try to create prefab from non-existent entity
    auto res = create_prefab_from_entity(em, static_cast<odyssey::EntityID>(999999), temp_dir_);
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.error().find("not found"), std::string::npos);
}

// Test unpack_prefab removes the instance entity
TEST_F(PrefabOpsTest, UnpackPrefabRemovesInstance) {
    EntityManager em;

    // Create a test entity (instance)
    odyssey::EntityID instance_id = em.create_entity("prefab_instance", "sphere");
    ASSERT_NE(em.get_entity(instance_id), nullptr);

    // Unpack it
    auto res = unpack_prefab(em, instance_id);
    ASSERT_TRUE(res.is_ok());

    // Verify the instance is gone
    EXPECT_TRUE(em.get_entity(instance_id) == nullptr);
}

// Test stub functions return expected error messages
TEST_F(PrefabOpsTest, OpenPrefabStubReturnsErr) {
    auto res = open_prefab_in_isolation(temp_dir_ / "nonexistent.prefab.xml");
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.error().find("not yet implemented"), std::string::npos);
}

TEST_F(PrefabOpsTest, ApplyOverridesStubReturnsErr) {
    EntityManager em;
    auto res = apply_prefab_overrides(em, 1);
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.error().find("not yet implemented"), std::string::npos);
}

TEST_F(PrefabOpsTest, RevertOverridesStubReturnsErr) {
    EntityManager em;
    auto res = revert_prefab_overrides(em, 1);
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.error().find("not yet implemented"), std::string::npos);
}

// Test unpack_prefab with non-existent entity returns Err
TEST_F(PrefabOpsTest, UnpackPrefabInvalidEntityReturnsErr) {
    EntityManager em;
    auto res = unpack_prefab(em, static_cast<odyssey::EntityID>(999999));
    ASSERT_FALSE(res.is_ok());
    EXPECT_NE(res.error().find("not found"), std::string::npos);
}
