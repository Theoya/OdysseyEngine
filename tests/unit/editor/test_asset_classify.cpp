// ---------------------------------------------------------------------------
// test_asset_classify.cpp
//
// Unit tests for classify_asset and filter_by_type pure helpers.
// Covers:
//   - filter_by_type: returns only assets of the requested type
//   - filter_by_type: empty input, no matches, all matches
//   - classify_asset: verify existing classification (duplicates from
//     test_asset_browser.cpp, but isolated for reference)
// No Vulkan, no ImGui, no networking — fast pure function tests.
// ---------------------------------------------------------------------------

#include "editor/asset_browser_panel.h"

#include <gtest/gtest.h>

using namespace odyssey::editor;

namespace {

// Helper: create an AssetEntry with a given type and relative path.
AssetEntry make_asset(const std::string& rel, AssetType type) {
    AssetEntry e;
    e.path = rel;
    e.relative = rel;
    e.type = type;
    e.size_bytes = 0;
    return e;
}

} // namespace

// ---------------------------------------------------------------------------
// filter_by_type
// ---------------------------------------------------------------------------

TEST(AssetClassify, FilterByTypeReturnsOnlyMatchingType) {
    std::vector<AssetEntry> entries = {
        make_asset("mesh1.mesh.xml", AssetType::Mesh),
        make_asset("mesh2.mesh.xml", AssetType::Mesh),
        make_asset("mat1.mat.xml", AssetType::Material),
        make_asset("prefab1.prefab.xml", AssetType::Prefab),
    };

    auto meshes = filter_by_type(entries, AssetType::Mesh);
    EXPECT_EQ(meshes.size(), 2);
    EXPECT_EQ(meshes[0].relative, "mesh1.mesh.xml");
    EXPECT_EQ(meshes[1].relative, "mesh2.mesh.xml");

    auto materials = filter_by_type(entries, AssetType::Material);
    EXPECT_EQ(materials.size(), 1);
    EXPECT_EQ(materials[0].relative, "mat1.mat.xml");

    auto prefabs = filter_by_type(entries, AssetType::Prefab);
    EXPECT_EQ(prefabs.size(), 1);
    EXPECT_EQ(prefabs[0].relative, "prefab1.prefab.xml");
}

TEST(AssetClassify, FilterByTypeEmptyInput) {
    std::vector<AssetEntry> entries;
    auto result = filter_by_type(entries, AssetType::Scene);
    EXPECT_EQ(result.size(), 0);
}

TEST(AssetClassify, FilterByTypeNoMatches) {
    std::vector<AssetEntry> entries = {
        make_asset("mesh1.mesh.xml", AssetType::Mesh),
        make_asset("mesh2.mesh.xml", AssetType::Mesh),
    };
    auto scenes = filter_by_type(entries, AssetType::Scene);
    EXPECT_EQ(scenes.size(), 0);
}

TEST(AssetClassify, FilterByTypeAllMatch) {
    std::vector<AssetEntry> entries = {
        make_asset("beh1.nadir", AssetType::Behavior),
        make_asset("beh2.nadir", AssetType::Behavior),
        make_asset("beh3.nadir", AssetType::Behavior),
    };
    auto behaviors = filter_by_type(entries, AssetType::Behavior);
    EXPECT_EQ(behaviors.size(), 3);
}

TEST(AssetClassify, FilterByTypePreservesOrder) {
    std::vector<AssetEntry> entries = {
        make_asset("z.mesh.xml", AssetType::Mesh),
        make_asset("a.mat.xml", AssetType::Material),
        make_asset("m.mesh.xml", AssetType::Mesh),
        make_asset("b.prefab.xml", AssetType::Prefab),
    };
    auto meshes = filter_by_type(entries, AssetType::Mesh);
    EXPECT_EQ(meshes.size(), 2);
    EXPECT_EQ(meshes[0].relative, "z.mesh.xml");
    EXPECT_EQ(meshes[1].relative, "m.mesh.xml");
}

// ---------------------------------------------------------------------------
// classify_asset (quick sanity check — full tests in test_asset_browser.cpp)
// ---------------------------------------------------------------------------

TEST(AssetClassify, ClassifySceneXml) {
    EXPECT_EQ(classify_asset("scene.scene.xml"), AssetType::Scene);
}

TEST(AssetClassify, ClassifyPrefabXml) {
    EXPECT_EQ(classify_asset("prefab.prefab.xml"), AssetType::Prefab);
}

TEST(AssetClassify, ClassifyMeshXml) {
    EXPECT_EQ(classify_asset("mesh.mesh.xml"), AssetType::Mesh);
}

TEST(AssetClassify, ClassifyMaterialXml) {
    EXPECT_EQ(classify_asset("material.mat.xml"), AssetType::Material);
}

TEST(AssetClassify, ClassifyBehaviorNadir) {
    EXPECT_EQ(classify_asset("behavior.nadir"), AssetType::Behavior);
}

TEST(AssetClassify, ClassifyMusicXml) {
    EXPECT_EQ(classify_asset("music.music.xml"), AssetType::Music);
}

TEST(AssetClassify, ClassifyLightingProfile) {
    EXPECT_EQ(classify_asset("lighting_profiles/liminal.xml"), AssetType::LightingProfile);
}

TEST(AssetClassify, ClassifyOther) {
    EXPECT_EQ(classify_asset("readme.txt"), AssetType::Other);
    EXPECT_EQ(classify_asset("config.json"), AssetType::Other);
}

TEST(AssetClassify, ClassifyIsCaseInsensitive) {
    EXPECT_EQ(classify_asset("SCENE.SCENE.XML"), AssetType::Scene);
    EXPECT_EQ(classify_asset("Behavior.NADIR"), AssetType::Behavior);
}
