// ---------------------------------------------------------------------------
// test_asset_browser.cpp
//
// Unit tests for the pure helpers in src/editor/asset_browser_panel.{h,cpp}.
// Covers:
//   - classify_asset: double-extension detection, context-sensitive .xml,
//     case insensitivity, unknown → Other.
//   - asset_type_order_key + sort_assets_canonical: UI ordering contract
//     (meshes < materials < prefabs < ...).
//   - enumerate_project: walks a real temp tree, skips non-asset files,
//     tolerates missing roots, returns canonical order.
// No Vulkan, no ImGui, no networking — fast.
// ---------------------------------------------------------------------------

#include "editor/asset_browser_panel.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace odyssey::editor;

namespace {

// Helper: touch a file with empty content inside `root`.
void touch(const std::filesystem::path& root, const std::string& rel) {
    auto p = root / rel;
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::out | std::ios::binary);
    f.put('x');
}

// A unique temp dir per test invocation so parallel runs don't collide.
std::filesystem::path make_temp_dir(const char* tag) {
    auto base = std::filesystem::temp_directory_path() /
                (std::string{"odyssey_ab_"} + tag + "_" +
                 std::to_string(::testing::UnitTest::GetInstance()
                                ->random_seed()));
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base);
    return base;
}

} // namespace

// ---------------------------------------------------------------------------
// classify_asset
// ---------------------------------------------------------------------------

TEST(AssetBrowserClassify, DoubleExtensionsTakePrecedence) {
    EXPECT_EQ(classify_asset("a/b/c.scene.xml"),    AssetType::Scene);
    EXPECT_EQ(classify_asset("a/b/c.prefab.xml"),   AssetType::Prefab);
    EXPECT_EQ(classify_asset("a/b/c.mesh.xml"),     AssetType::Mesh);
    EXPECT_EQ(classify_asset("a/b/c.mat.xml"),      AssetType::Material);
    EXPECT_EQ(classify_asset("a/b/c.music.xml"),    AssetType::Music);
    EXPECT_EQ(classify_asset("a/b/c.actions.xml"),  AssetType::Actions);
    EXPECT_EQ(classify_asset("a/b/c.skeleton.xml"), AssetType::Skeleton);
    EXPECT_EQ(classify_asset("a/b/c.anim.xml"),     AssetType::Animation);
}

TEST(AssetBrowserClassify, NadirIsBehavior) {
    EXPECT_EQ(classify_asset("behaviors/scout.nadir"), AssetType::Behavior);
    EXPECT_EQ(classify_asset("demo/showcase/behaviors/brute.nadir"),
              AssetType::Behavior);
}

TEST(AssetBrowserClassify, PlainXmlDisambiguatesByDirectory) {
    EXPECT_EQ(classify_asset("demo/showcase/lighting_profiles/liminal.xml"),
              AssetType::LightingProfile);
    EXPECT_EQ(classify_asset("demo/showcase/music/leitmotifs.xml"),
              AssetType::Music);
    // No directory hint → Other.
    EXPECT_EQ(classify_asset("demo/showcase/whatever.xml"),
              AssetType::Other);
}

TEST(AssetBrowserClassify, CaseInsensitive) {
    EXPECT_EQ(classify_asset("X.SCENE.XML"),  AssetType::Scene);
    EXPECT_EQ(classify_asset("Y.Prefab.Xml"), AssetType::Prefab);
    EXPECT_EQ(classify_asset("Z.NADIR"),      AssetType::Behavior);
}

TEST(AssetBrowserClassify, UnknownExtensionsFallToOther) {
    EXPECT_EQ(classify_asset("a/readme.md"),   AssetType::Other);
    EXPECT_EQ(classify_asset("a/icon.png"),    AssetType::Other);
    EXPECT_EQ(classify_asset("a/config.json"), AssetType::Other);
    EXPECT_EQ(classify_asset(""),              AssetType::Other);
}

// ---------------------------------------------------------------------------
// asset_type_order_key + sort_assets_canonical
// ---------------------------------------------------------------------------

TEST(AssetBrowserOrder, MeshesBeforeMaterialsBeforePrefabs) {
    EXPECT_LT(asset_type_order_key(AssetType::Mesh),
              asset_type_order_key(AssetType::Material));
    EXPECT_LT(asset_type_order_key(AssetType::Material),
              asset_type_order_key(AssetType::Prefab));
}

TEST(AssetBrowserOrder, PrefabsBeforeBehaviors) {
    EXPECT_LT(asset_type_order_key(AssetType::Prefab),
              asset_type_order_key(AssetType::Behavior));
}

TEST(AssetBrowserOrder, OtherIsLast) {
    // Every known type sorts before Other.
    for (auto t : { AssetType::Mesh, AssetType::Material, AssetType::Prefab,
                    AssetType::Behavior, AssetType::LightingProfile,
                    AssetType::Music, AssetType::Scene, AssetType::Actions,
                    AssetType::Skeleton, AssetType::Animation }) {
        EXPECT_LT(asset_type_order_key(t),
                  asset_type_order_key(AssetType::Other));
    }
}

TEST(AssetBrowserOrder, SortCanonicalGroupsByTypeThenPath) {
    std::vector<AssetEntry> v;
    auto mk = [](const char* rel, AssetType t) {
        AssetEntry e;
        e.relative = rel;
        e.path     = rel;
        e.type     = t;
        return e;
    };
    v.push_back(mk("zzz.prefab.xml", AssetType::Prefab));
    v.push_back(mk("aaa.mat.xml",    AssetType::Material));
    v.push_back(mk("bbb.mesh.xml",   AssetType::Mesh));
    v.push_back(mk("aaa.mesh.xml",   AssetType::Mesh));
    v.push_back(mk("mid.prefab.xml", AssetType::Prefab));

    sort_assets_canonical(v);

    // Meshes first (alphabetical within group), then materials, then prefabs.
    ASSERT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0].type, AssetType::Mesh);
    EXPECT_EQ(v[0].relative, "aaa.mesh.xml");
    EXPECT_EQ(v[1].type, AssetType::Mesh);
    EXPECT_EQ(v[1].relative, "bbb.mesh.xml");
    EXPECT_EQ(v[2].type, AssetType::Material);
    EXPECT_EQ(v[3].type, AssetType::Prefab);
    EXPECT_EQ(v[3].relative, "mid.prefab.xml");
    EXPECT_EQ(v[4].type, AssetType::Prefab);
    EXPECT_EQ(v[4].relative, "zzz.prefab.xml");
}

// ---------------------------------------------------------------------------
// enumerate_project — touches the filesystem
// ---------------------------------------------------------------------------

TEST(AssetBrowserEnumerate, MissingRootReturnsEmpty) {
    auto out = enumerate_project("Z:/this/path/does/not/exist");
    EXPECT_TRUE(out.empty());
}

TEST(AssetBrowserEnumerate, EmptyPathReturnsEmpty) {
    auto out = enumerate_project(std::filesystem::path{});
    EXPECT_TRUE(out.empty());
}

TEST(AssetBrowserEnumerate, WalksRecursivelyAndClassifies) {
    auto root = make_temp_dir("walk");
    touch(root, "meshes/crate.mesh.xml");
    touch(root, "meshes/pillar.mesh.xml");
    touch(root, "materials/gold.mat.xml");
    touch(root, "prefabs/scout.prefab.xml");
    touch(root, "behaviors/scout.nadir");
    touch(root, "lighting_profiles/liminal.xml");
    touch(root, "music/showcase.music.xml");
    touch(root, "showcase.scene.xml");
    touch(root, "actions/scout_patrol.actions.xml");
    // Unknowns — should NOT appear in the enumeration (Other is skipped).
    touch(root, "README.md");
    touch(root, "notes.txt");

    auto out = enumerate_project(root);

    // Expect exactly 9 known-type files.
    EXPECT_EQ(out.size(), 9u);

    // Check the canonical ordering: the first entries must be the meshes.
    ASSERT_GE(out.size(), 2u);
    EXPECT_EQ(out[0].type, AssetType::Mesh);
    EXPECT_EQ(out[1].type, AssetType::Mesh);
    // Last known entry must be the Actions type (index 7 in the key table).
    EXPECT_EQ(out.back().type, AssetType::Actions);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(AssetBrowserEnumerate, RelativePathsAreRootAnchored) {
    auto root = make_temp_dir("relpath");
    touch(root, "sub/deep/crate.mesh.xml");
    auto out = enumerate_project(root);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].relative.generic_string(), "sub/deep/crate.mesh.xml");
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// Group label contract — tree headers must match expected user-facing text.
// ---------------------------------------------------------------------------

TEST(AssetBrowserGroupLabel, AllTypesHaveLabels) {
    EXPECT_STREQ(asset_type_group_label(AssetType::Mesh),            "Meshes");
    EXPECT_STREQ(asset_type_group_label(AssetType::Material),        "Materials");
    EXPECT_STREQ(asset_type_group_label(AssetType::Prefab),          "Prefabs");
    EXPECT_STREQ(asset_type_group_label(AssetType::Behavior),        "Behaviors (.nadir)");
    EXPECT_STREQ(asset_type_group_label(AssetType::LightingProfile), "Lighting Profiles");
    EXPECT_STREQ(asset_type_group_label(AssetType::Music),           "Music");
    EXPECT_STREQ(asset_type_group_label(AssetType::Scene),           "Scenes");
    EXPECT_STREQ(asset_type_group_label(AssetType::Actions),         "Actions");
}
