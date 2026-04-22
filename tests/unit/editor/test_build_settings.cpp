// ---------------------------------------------------------------------------
// test_build_settings.cpp
//
// Unit tests for src/editor/build_settings.{h,cpp}.
// Covers:
//   - Valid XML round-trip (load + save)
//   - Malformed XML returns error
//   - Missing required attributes returns error
//   - Unknown target enum returns error
//   - Empty scene list returns error
//   - Multiple scenes preserve order
// ---------------------------------------------------------------------------

#include "editor/build_settings.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace odyssey::editor;

namespace {

// Temporary test file helper.
class TempXMLFile {
public:
    TempXMLFile() {
        path_ = std::filesystem::temp_directory_path() / "test_build_settings.xml";
    }

    ~TempXMLFile() {
        std::filesystem::remove(path_);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace

// ---------------------------------------------------------------------------
// Valid XML round-trip
// ---------------------------------------------------------------------------

TEST(BuildSettings, ValidRoundTrip) {
    TempXMLFile temp;

    // Create settings.
    BuildSettings orig{
        .target = "odyssey_shooter",
        .config = "Release",
        .output_dir = "../dist",
        .scenes = {"demo/scene1.xml", "demo/scene2.xml"},
        .defines = {{"FOO", "1"}, {"BAR", "hello"}}
    };

    // Save.
    auto save_result = save_build_settings(temp.path(), orig);
    EXPECT_TRUE(save_result.is_ok());

    // Load.
    auto load_result = load_build_settings(temp.path());
    EXPECT_TRUE(load_result.is_ok());

    auto loaded = load_result.value();
    EXPECT_EQ(loaded.target, orig.target);
    EXPECT_EQ(loaded.config, orig.config);
    EXPECT_EQ(loaded.output_dir, orig.output_dir);
    EXPECT_EQ(loaded.scenes, orig.scenes);
    EXPECT_EQ(loaded.defines, orig.defines);
}

// ---------------------------------------------------------------------------
// Malformed XML
// ---------------------------------------------------------------------------

TEST(BuildSettings, MalformedXMLReturnsError) {
    TempXMLFile temp;

    // Write invalid XML.
    std::ofstream f(temp.path());
    f << "<?xml version=\"1.0\"?><invalid>{{{";
    f.close();

    auto result = load_build_settings(temp.path());
    EXPECT_TRUE(result.is_err());
}

// ---------------------------------------------------------------------------
// Missing required attributes
// ---------------------------------------------------------------------------

TEST(BuildSettings, MissingTargetAttr) {
    TempXMLFile temp;

    std::ofstream f(temp.path());
    f << "<?xml version=\"1.0\"?>\n";
    f << "<build_settings config=\"Release\">\n";
    f << "  <scenes><scene path=\"test.xml\"/></scenes>\n";
    f << "</build_settings>\n";
    f.close();

    auto result = load_build_settings(temp.path());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(result.error().find("target"), std::string::npos);
}

TEST(BuildSettings, MissingSceneElement) {
    TempXMLFile temp;

    std::ofstream f(temp.path());
    f << "<?xml version=\"1.0\"?>\n";
    f << "<build_settings target=\"odyssey_shooter\"/>\n";
    f.close();

    auto result = load_build_settings(temp.path());
    EXPECT_TRUE(result.is_err());
}

// ---------------------------------------------------------------------------
// Invalid enum values
// ---------------------------------------------------------------------------

TEST(BuildSettings, InvalidTargetReturnsError) {
    TempXMLFile temp;

    std::ofstream f(temp.path());
    f << "<?xml version=\"1.0\"?>\n";
    f << "<build_settings target=\"invalid_target\">\n";
    f << "  <scenes><scene path=\"test.xml\"/></scenes>\n";
    f << "</build_settings>\n";
    f.close();

    auto result = load_build_settings(temp.path());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(result.error().find("Invalid target"), std::string::npos);
}

TEST(BuildSettings, InvalidConfigReturnsError) {
    TempXMLFile temp;

    std::ofstream f(temp.path());
    f << "<?xml version=\"1.0\"?>\n";
    f << "<build_settings target=\"odyssey_shooter\" config=\"BadConfig\">\n";
    f << "  <scenes><scene path=\"test.xml\"/></scenes>\n";
    f << "</build_settings>\n";
    f.close();

    auto result = load_build_settings(temp.path());
    EXPECT_TRUE(result.is_err());
}

// ---------------------------------------------------------------------------
// Empty scene list
// ---------------------------------------------------------------------------

TEST(BuildSettings, EmptySceneListReturnsError) {
    TempXMLFile temp;

    std::ofstream f(temp.path());
    f << "<?xml version=\"1.0\"?>\n";
    f << "<build_settings target=\"odyssey_shooter\">\n";
    f << "  <scenes></scenes>\n";
    f << "</build_settings>\n";
    f.close();

    auto result = load_build_settings(temp.path());
    EXPECT_TRUE(result.is_err());
    EXPECT_NE(result.error().find("No scenes"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Multiple scenes preserve order
// ---------------------------------------------------------------------------

TEST(BuildSettings, MultipleScenesPreserveOrder) {
    TempXMLFile temp;

    std::ofstream f(temp.path());
    f << "<?xml version=\"1.0\"?>\n";
    f << "<build_settings target=\"odyssey_fps\">\n";
    f << "  <scenes>\n";
    f << "    <scene path=\"first.xml\"/>\n";
    f << "    <scene path=\"second.xml\"/>\n";
    f << "    <scene path=\"third.xml\"/>\n";
    f << "  </scenes>\n";
    f << "</build_settings>\n";
    f.close();

    auto result = load_build_settings(temp.path());
    EXPECT_TRUE(result.is_ok());

    auto loaded = result.value();
    EXPECT_EQ(loaded.scenes.size(), 3);
    EXPECT_EQ(loaded.scenes[0], "first.xml");
    EXPECT_EQ(loaded.scenes[1], "second.xml");
    EXPECT_EQ(loaded.scenes[2], "third.xml");
}
