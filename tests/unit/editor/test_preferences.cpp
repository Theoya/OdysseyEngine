// ---------------------------------------------------------------------------
// test_preferences.cpp
//
// Unit tests for src/editor/preferences.{h,cpp}.
// Covers:
//   - Default round-trip (file doesn't exist)
//   - Non-default values round-trip
//   - Malformed XML returns error
//   - Missing file returns defaults
// ---------------------------------------------------------------------------

#include "editor/preferences.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace odyssey::editor;

namespace {

// Temporary directory helper.
class TempDir {
public:
    TempDir() {
        dir_ = std::filesystem::temp_directory_path() / "test_prefs";
        std::filesystem::create_directories(dir_);
    }

    ~TempDir() {
        std::filesystem::remove_all(dir_);
    }

    const std::filesystem::path& path() const { return dir_; }

private:
    std::filesystem::path dir_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Default values round-trip
// ---------------------------------------------------------------------------

TEST(Preferences, DefaultRoundTrip) {
    TempDir temp;

    Preferences orig;  // Default values

    auto save_result = save_preferences(temp.path(), orig);
    EXPECT_TRUE(save_result.is_ok());

    auto load_result = load_preferences(temp.path());
    EXPECT_TRUE(load_result.is_ok());

    auto loaded = load_result.value();
    EXPECT_EQ(loaded.editor_font_size, orig.editor_font_size);
    EXPECT_EQ(loaded.scene_camera_base_speed, orig.scene_camera_base_speed);
    EXPECT_EQ(loaded.position_snap, orig.position_snap);
    EXPECT_EQ(loaded.rotation_snap_deg, orig.rotation_snap_deg);
    EXPECT_EQ(loaded.scale_snap, orig.scale_snap);
    EXPECT_EQ(loaded.autosave_interval_sec, orig.autosave_interval_sec);
    EXPECT_EQ(loaded.dark_theme, orig.dark_theme);
}

// ---------------------------------------------------------------------------
// Non-default values round-trip
// ---------------------------------------------------------------------------

TEST(Preferences, NonDefaultRoundTrip) {
    TempDir temp;

    Preferences orig{
        .editor_font_size = 18.0f,
        .scene_camera_base_speed = 25.0f,
        .position_snap = 0.5f,
        .rotation_snap_deg = 30.0f,
        .scale_snap = 0.2f,
        .autosave_interval_sec = 120,
        .dark_theme = false
    };

    auto save_result = save_preferences(temp.path(), orig);
    EXPECT_TRUE(save_result.is_ok());

    auto load_result = load_preferences(temp.path());
    EXPECT_TRUE(load_result.is_ok());

    auto loaded = load_result.value();
    EXPECT_EQ(loaded.editor_font_size, 18.0f);
    EXPECT_EQ(loaded.scene_camera_base_speed, 25.0f);
    EXPECT_EQ(loaded.position_snap, 0.5f);
    EXPECT_EQ(loaded.rotation_snap_deg, 30.0f);
    EXPECT_EQ(loaded.scale_snap, 0.2f);
    EXPECT_EQ(loaded.autosave_interval_sec, 120);
    EXPECT_EQ(loaded.dark_theme, false);
}

// ---------------------------------------------------------------------------
// Malformed XML
// ---------------------------------------------------------------------------

TEST(Preferences, MalformedXMLReturnsError) {
    TempDir temp;

    auto xml_path = temp.path() / "editor_preferences.xml";
    std::ofstream f(xml_path);
    f << "<?xml version=\"1.0\"?><invalid>{{{";
    f.close();

    auto result = load_preferences(temp.path());
    EXPECT_TRUE(result.is_err());
}

// ---------------------------------------------------------------------------
// Missing file returns defaults
// ---------------------------------------------------------------------------

TEST(Preferences, MissingFileReturnsDefaults) {
    TempDir temp;

    auto result = load_preferences(temp.path());
    EXPECT_TRUE(result.is_ok());

    auto loaded = result.value();
    EXPECT_EQ(loaded.editor_font_size, 14.0f);
    EXPECT_EQ(loaded.scene_camera_base_speed, 10.0f);
    EXPECT_EQ(loaded.position_snap, 0.25f);
    EXPECT_EQ(loaded.rotation_snap_deg, 15.0f);
    EXPECT_EQ(loaded.scale_snap, 0.1f);
    EXPECT_EQ(loaded.autosave_interval_sec, 0);
    EXPECT_EQ(loaded.dark_theme, true);
}
