#include <gtest/gtest.h>
#include "editor/editor_prefs.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>

using odyssey::editor::EditorPrefs;
using odyssey::editor::load_editor_prefs;
using odyssey::editor::save_editor_prefs;

class EditorPrefsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test prefs files.
        temp_dir_ = std::filesystem::temp_directory_path() / "odyssey_editor_prefs_test";
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        // Clean up temp directory
        std::error_code ec;
        std::filesystem::remove_all(temp_dir_, ec);
    }

    std::filesystem::path temp_dir_;
};

TEST_F(EditorPrefsTest, LoadNonexistentFileReturnsEmptyPrefs) {
    auto r = load_editor_prefs(temp_dir_);
    ASSERT_TRUE(r.is_ok());
    auto prefs = r.value();
    EXPECT_TRUE(prefs.recent_scenes.empty());
    EXPECT_TRUE(prefs.active_layout.empty());
}

TEST_F(EditorPrefsTest, RoundTripSaveLoad) {
    EditorPrefs original;
    original.recent_scenes = {"scene1.xml", "scene2.xml", "scene3.xml"};
    original.active_layout = "default_layout";

    // Save
    auto save_r = save_editor_prefs(temp_dir_, original);
    ASSERT_TRUE(save_r.is_ok());

    // Load back
    auto load_r = load_editor_prefs(temp_dir_);
    ASSERT_TRUE(load_r.is_ok());
    auto loaded = load_r.value();

    EXPECT_EQ(loaded.recent_scenes.size(), 3);
    EXPECT_EQ(loaded.recent_scenes[0], "scene1.xml");
    EXPECT_EQ(loaded.recent_scenes[1], "scene2.xml");
    EXPECT_EQ(loaded.recent_scenes[2], "scene3.xml");
    EXPECT_EQ(loaded.active_layout, "default_layout");
}

TEST_F(EditorPrefsTest, MalformedXmlReturnsErr) {
    // Write broken XML to prefs file
    std::filesystem::path prefs_path = temp_dir_ / "editor_prefs.xml";
    std::ofstream f(prefs_path);
    f << "<editor_prefs><broken>";
    f.close();

    auto r = load_editor_prefs(temp_dir_);
    ASSERT_TRUE(r.is_err());
}

TEST_F(EditorPrefsTest, DeduplicateAndTruncate) {
    EditorPrefs prefs;
    // Manually add 10 scenes
    for (int i = 0; i < 10; ++i) {
        prefs.recent_scenes.push_back("scene" + std::to_string(i) + ".xml");
    }

    // Save and load
    auto save_r = save_editor_prefs(temp_dir_, prefs);
    ASSERT_TRUE(save_r.is_ok());

    auto load_r = load_editor_prefs(temp_dir_);
    ASSERT_TRUE(load_r.is_ok());
    auto loaded = load_r.value();

    // Should have truncated to 8
    EXPECT_EQ(loaded.recent_scenes.size(), 8);
}
