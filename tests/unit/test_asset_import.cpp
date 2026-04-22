#include "editor/asset_import.h"
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>

using namespace odyssey::editor;

// Test 1: .obj plans to meshes/<name>.mesh.xml + copy target
TEST(AssetImportTest, ObjPlansMeshesWithDescriptor) {
    ImportSource source{std::filesystem::path("C:/test/mycrate.obj")};
    auto project_root = std::filesystem::path("D:/project");

    auto result = plan_import(source, project_root);
    ASSERT_TRUE(result.is_ok());

    auto target = result.value();
    EXPECT_EQ(target.type, AssetType::Mesh);
    EXPECT_EQ(target.target_abs, project_root / "meshes" / "mycrate.obj");
    EXPECT_TRUE(target.descriptor_abs.has_value());
    EXPECT_EQ(target.descriptor_abs.value(), project_root / "meshes" / "mycrate.mesh.xml");
}

// Test 2: .png plans to textures/<name>.png
TEST(AssetImportTest, PngPlanTextures) {
    ImportSource source{std::filesystem::path("C:/test/brick.png")};
    auto project_root = std::filesystem::path("D:/project");

    auto result = plan_import(source, project_root);
    ASSERT_TRUE(result.is_ok());

    auto target = result.value();
    EXPECT_EQ(target.target_abs, project_root / "textures" / "brick.png");
    EXPECT_FALSE(target.descriptor_abs.has_value());
}

// Test 3: .wav plans to audio/<name>.wav
TEST(AssetImportTest, WavPlansAudio) {
    ImportSource source{std::filesystem::path("C:/test/explosion.wav")};
    auto project_root = std::filesystem::path("D:/project");

    auto result = plan_import(source, project_root);
    ASSERT_TRUE(result.is_ok());

    auto target = result.value();
    EXPECT_EQ(target.target_abs, project_root / "audio" / "explosion.wav");
    EXPECT_FALSE(target.descriptor_abs.has_value());
}

// Test 4: .glb plans to imported/<name>.glb
TEST(AssetImportTest, GlbPlansImported) {
    ImportSource source{std::filesystem::path("C:/test/model.glb")};
    auto project_root = std::filesystem::path("D:/project");

    auto result = plan_import(source, project_root);
    ASSERT_TRUE(result.is_ok());

    auto target = result.value();
    EXPECT_EQ(target.target_abs, project_root / "imported" / "model.glb");
    EXPECT_FALSE(target.descriptor_abs.has_value());
}

// Test 5: Unknown .xyz returns Err
TEST(AssetImportTest, UnknownExtensionReturnsErr) {
    ImportSource source{std::filesystem::path("C:/test/unknown.xyz")};
    auto project_root = std::filesystem::path("D:/project");

    auto result = plan_import(source, project_root);
    ASSERT_FALSE(result.is_ok());
    EXPECT_NE(result.error().find("unsupported extension"), std::string::npos);
}

// Test 6: Empty source path returns Err
TEST(AssetImportTest, EmptySourceReturnsErr) {
    ImportSource source{std::filesystem::path("")};
    auto project_root = std::filesystem::path("D:/project");

    auto result = plan_import(source, project_root);
    ASSERT_FALSE(result.is_ok());
    EXPECT_NE(result.error().find("source path is empty"), std::string::npos);
}

// Test 7: generate_mesh_xml_for_obj produces valid XML
TEST(AssetImportTest, GenerateMeshXmlForObjProducesValidXml) {
    auto xml = generate_mesh_xml_for_obj("meshes/mycrate.obj");

    // Check for required elements
    EXPECT_NE(xml.find("<?xml version=\"1.0\""), std::string::npos);
    EXPECT_NE(xml.find("<mesh"), std::string::npos);
    EXPECT_NE(xml.find("<source"), std::string::npos);
    EXPECT_NE(xml.find("format=\"obj\""), std::string::npos);
    EXPECT_NE(xml.find("meshes/mycrate.obj"), std::string::npos);
    EXPECT_NE(xml.find("<lod>"), std::string::npos);
    EXPECT_NE(xml.find("<collider"), std::string::npos);
    EXPECT_NE(xml.find("</mesh>"), std::string::npos);
}

// Test 8: execute_import copies files and writes descriptor (integration)
TEST(AssetImportTest, ExecuteImportCopiesAndCreatesDescriptor) {
    // Create a temporary project root
    auto tmp_root = std::filesystem::temp_directory_path() /
                    ("odyssey_import_test_" + std::to_string(std::random_device{}()));

    // Create the test project structure
    std::filesystem::create_directories(tmp_root);
    std::filesystem::create_directories(tmp_root / "meshes");

    // Create a temporary source .obj file
    auto tmp_source = std::filesystem::temp_directory_path() / "test_import_source.obj";
    {
        std::ofstream out(tmp_source);
        out << "# Test OBJ file\nv 0 0 0\nv 1 0 0\nv 0 1 0\n";
        out.close();
    }

    ImportSource source{tmp_source};

    auto result = execute_import(source, tmp_root, false);
    ASSERT_TRUE(result.is_ok()) << result.error();

    auto target = result.value();

    // Check that the target file was copied
    EXPECT_TRUE(std::filesystem::exists(target.target_abs));
    EXPECT_EQ(target.target_abs, tmp_root / "meshes" / "test_import_source.obj");

    // Check that the descriptor was created
    EXPECT_TRUE(target.descriptor_abs.has_value());
    EXPECT_TRUE(std::filesystem::exists(target.descriptor_abs.value()));

    // Verify descriptor content
    {
        std::ifstream desc_file(target.descriptor_abs.value());
        std::string desc_content((std::istreambuf_iterator<char>(desc_file)),
                                 std::istreambuf_iterator<char>());
        EXPECT_NE(desc_content.find("<mesh"), std::string::npos);
        EXPECT_NE(desc_content.find("format=\"obj\""), std::string::npos);
        desc_file.close();
    }

    // Cleanup — close all file handles before removing directories
    std::error_code ec;
    std::filesystem::remove_all(tmp_root, ec);
    std::filesystem::remove(tmp_source, ec);
}

// Test 9: execute_import rejects existing target without overwrite flag
TEST(AssetImportTest, ExecuteImportRejectsExistingWithoutOverwrite) {
    auto tmp_root = std::filesystem::temp_directory_path() /
                    ("odyssey_import_test_dup_" + std::to_string(std::random_device{}()));

    std::filesystem::create_directories(tmp_root / "meshes");

    // Create source
    auto tmp_source = std::filesystem::temp_directory_path() / "test_dup_source.obj";
    {
        std::ofstream out(tmp_source);
        out << "# Test\n";
        out.close();
    }

    // Create target file (already exists)
    auto target_path = tmp_root / "meshes" / "test_dup_source.obj";
    {
        std::ofstream out(target_path);
        out << "# Existing\n";
        out.close();
    }

    ImportSource source{tmp_source};
    auto result = execute_import(source, tmp_root, false);

    EXPECT_FALSE(result.is_ok());
    EXPECT_NE(result.error().find("already exists"), std::string::npos);

    // Cleanup
    std::error_code ec;
    std::filesystem::remove_all(tmp_root, ec);
    std::filesystem::remove(tmp_source, ec);
}

// Test 10: classify_import_source recognizes all supported types
TEST(AssetImportTest, ClassifyImportSourceRecognizesTypes) {
    auto obj_result = classify_import_source("file.obj");
    ASSERT_TRUE(obj_result.is_ok());
    EXPECT_EQ(obj_result.value(), AssetType::Mesh);

    auto png_result = classify_import_source("image.png");
    ASSERT_TRUE(png_result.is_ok());

    auto wav_result = classify_import_source("sound.wav");
    ASSERT_TRUE(wav_result.is_ok());

    auto glb_result = classify_import_source("model.glb");
    ASSERT_TRUE(glb_result.is_ok());

    auto xml_result = classify_import_source("data.xml");
    ASSERT_TRUE(xml_result.is_ok());
}

// Test 11: classify_import_source rejects unknown types
TEST(AssetImportTest, ClassifyImportSourceRejectsUnknown) {
    auto result = classify_import_source("file.unknown");
    EXPECT_FALSE(result.is_ok());
    EXPECT_NE(result.error().find("unsupported extension"), std::string::npos);
}

// Test 12: .jpg and .jpeg both work
TEST(AssetImportTest, JpgAndJpegSupported) {
    auto jpg_result = plan_import(ImportSource{"C:/test/photo.jpg"}, "D:/proj");
    ASSERT_TRUE(jpg_result.is_ok());
    EXPECT_EQ(jpg_result.value().target_abs, std::filesystem::path("D:/proj/textures/photo.jpg"));

    auto jpeg_result = plan_import(ImportSource{"C:/test/photo.jpeg"}, "D:/proj");
    ASSERT_TRUE(jpeg_result.is_ok());
    EXPECT_EQ(jpeg_result.value().target_abs, std::filesystem::path("D:/proj/textures/photo.jpeg"));
}

// Test 13: .fbx plans to imported/
TEST(AssetImportTest, FbxPlansImported) {
    ImportSource source{std::filesystem::path("C:/test/model.fbx")};
    auto project_root = std::filesystem::path("D:/project");

    auto result = plan_import(source, project_root);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().target_abs, project_root / "imported" / "model.fbx");
}
