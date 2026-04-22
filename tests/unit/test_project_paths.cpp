#include <gtest/gtest.h>
#include "editor/project_paths.h"

using odyssey::editor::resolve_project_paths;
using odyssey::editor::ProjectPaths;

TEST(ProjectPathsTest, AbsExeDirEmptyCliResolvesShowcase) {
    auto r = resolve_project_paths("C:/odyssey/build/Release", "");
    ASSERT_TRUE(r.is_ok());
    auto p = r.value();
    EXPECT_EQ(p.exe_dir, std::filesystem::path("C:/odyssey/build/Release"));
    EXPECT_EQ(p.showcase_scene,
              std::filesystem::path("C:/odyssey/build/Release/demo/showcase/showcase.scene.xml"));
    EXPECT_EQ(p.asset_root,
              std::filesystem::path("C:/odyssey/build/Release/demo/showcase"));
    EXPECT_EQ(p.layouts_dir,
              std::filesystem::path("C:/odyssey/build/Release/layouts"));
}

TEST(ProjectPathsTest, AbsoluteCliSceneOverridesShowcase) {
    auto r = resolve_project_paths("C:/odyssey/build/Release",
                                   "D:/custom/other.scene.xml");
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.value().showcase_scene,
              std::filesystem::path("D:/custom/other.scene.xml"));
}

TEST(ProjectPathsTest, RelativeCliSceneResolvedAgainstExeDir) {
    auto r = resolve_project_paths("C:/odyssey/build/Release",
                                   "demo/fps_humanoid/scenes/fps_arena.scene.xml");
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.value().showcase_scene,
              std::filesystem::path("C:/odyssey/build/Release/demo/fps_humanoid/scenes/fps_arena.scene.xml"));
}

TEST(ProjectPathsTest, EmptyExeDirIsErr) {
    auto r = resolve_project_paths("", "");
    ASSERT_TRUE(r.is_err());
}

TEST(ProjectPathsTest, RelativeExeDirIsErr) {
    auto r = resolve_project_paths("build/Release", "");
    ASSERT_TRUE(r.is_err());
}
