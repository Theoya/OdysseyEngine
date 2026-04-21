#include <gtest/gtest.h>

#include "core/types.h"
#include "nadir/nadir_system.h"
#include "nadir/behavior_compiler.h"
#include "nadir/nadir_buffers.h"
#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/buffer.h"
#include "vulkan/compute_pipeline.h"
#include "vulkan/command.h"

#include <vector>

// ---------------------------------------------------------------------------
// Pipeline tests exercise the full Nadir data flow:
//   input SSBOs  -->  compute dispatch  -->  output readback
//
// They require a Vulkan-capable GPU.  Tests that cannot find one are skipped.
// ---------------------------------------------------------------------------

class NadirPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a headless Vulkan instance (no surface — compute only).
        auto inst_config = odyssey::vulkan::compute_instance_config(/*validation=*/true);
        inst_config.required_extensions.clear();  // no surface extensions

        auto inst_result = odyssey::vulkan::create_instance(inst_config);
        if (inst_result.is_err()) {
            GTEST_SKIP() << "No Vulkan support: " << inst_result.error();
            return;
        }
        instance_ = inst_result.value();

        // Enumerate physical devices.
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
        if (device_count == 0) {
            GTEST_SKIP() << "No Vulkan-capable devices found";
            return;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

        gpu_available_ = true;
    }

    void TearDown() override {
        if (instance_ != VK_NULL_HANDLE) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }
    }

    VkInstance instance_   = VK_NULL_HANDLE;
    bool gpu_available_    = false;
};

// ---------------------------------------------------------------------------
// Shader compilation (does not require GPU)
// ---------------------------------------------------------------------------

TEST_F(NadirPipelineTest, ShaderCompilesAndProducesSpirV) {
    std::string nadir_source = R"(
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= total_entities) return;

    float hp     = stats[idx].health;
    float max_hp = stats[idx].max_health;
    float health_score = hp / max(max_hp, 0.001);

    outputs[idx].move_vector     = vec4(0.0, 0.0, 1.0, health_score);
    outputs[idx].attack_target   = vec4(0.0);
    outputs[idx].animation_id    = 1u;
    outputs[idx].animation_blend = health_score;
    outputs[idx].sound_event     = 0u;
    outputs[idx].sound_priority  = 0.0;
    outputs[idx].comms_signal    = 0.0;
    outputs[idx].comms_urgency   = 0.0;
}
)";
    auto full_source = odyssey::nadir::prepare_shader_source(nadir_source);
    auto bytecode =
        odyssey::nadir::compile_behavior_shader(full_source, "test_pipeline", "");
    ASSERT_TRUE(bytecode.success) << bytecode.error_message;
    EXPECT_GT(bytecode.spirv.size(), 0u);
}

// ---------------------------------------------------------------------------
// Buffer layout matches GPU alignment expectations
// ---------------------------------------------------------------------------

TEST_F(NadirPipelineTest, BufferLayoutMatchesGPURequirements) {
    odyssey::nadir::ArchetypeBufferDesc desc{
        .entity_count      = 4,
        .needs_spatial_grid = false,
        .needs_debug_output = true
    };
    auto layout = odyssey::nadir::compute_buffer_set_layout(desc);

    // 4 entities * 16 bytes / position = 64 bytes
    EXPECT_EQ(layout.transform_size, 64u);
    // 4 entities * 32 bytes / stats   = 128 bytes
    EXPECT_EQ(layout.stats_size, 128u);
    // 4 entities * 64 bytes / output  = 256 bytes
    EXPECT_EQ(layout.output_size, 256u);
}

// ---------------------------------------------------------------------------
// Archetype name extraction — pure helper
// ---------------------------------------------------------------------------

TEST_F(NadirPipelineTest, ArchetypeNameFromPath) {
    auto name = odyssey::nadir::archetype_name_from_path(
        "behaviors/shaders/enemy_pack_hunter.nadir");
    EXPECT_EQ(name, "enemy_pack_hunter");
}

TEST_F(NadirPipelineTest, ArchetypeNameFromPath_Simple) {
    auto name = odyssey::nadir::archetype_name_from_path("test_flock.nadir");
    EXPECT_EQ(name, "test_flock");
}

// ---------------------------------------------------------------------------
// Standalone (no fixture) tests — dispatch config
// ---------------------------------------------------------------------------

TEST(NadirPipelineStandalone, DispatchConfigFor4Agents) {
    auto config = odyssey::vulkan::compute_dispatch_config(4, 256);
    EXPECT_EQ(config.group_count_x, 1u);
}

TEST(NadirPipelineStandalone, DispatchConfigFor1000Agents) {
    auto config = odyssey::vulkan::compute_dispatch_config(1000, 256);
    EXPECT_EQ(config.group_count_x, 4u);  // ceil(1000/256) = 4
}

TEST(NadirPipelineStandalone, DispatchConfigMaxAgents) {
    auto config = odyssey::vulkan::compute_dispatch_config(100000, 256);
    EXPECT_EQ(config.group_count_x, 391u);  // ceil(100000/256) = 391
    EXPECT_EQ(config.group_count_y, 1u);
    EXPECT_EQ(config.group_count_z, 1u);
}

// ---------------------------------------------------------------------------
// Buffer layout edge cases for pipeline context
// ---------------------------------------------------------------------------

TEST(NadirPipelineStandalone, BufferLayoutSingleAgent) {
    odyssey::nadir::ArchetypeBufferDesc desc{
        .entity_count      = 1,
        .needs_spatial_grid = false,
        .needs_debug_output = false
    };
    auto layout = odyssey::nadir::compute_buffer_set_layout(desc);
    EXPECT_EQ(layout.transform_size, 16u);
    EXPECT_EQ(layout.stats_size, 32u);
    EXPECT_EQ(layout.output_size, 64u);
    EXPECT_EQ(layout.persist_size, 64u);
    EXPECT_EQ(layout.debug_size, 0u);
    EXPECT_EQ(layout.spatial_size, 0u);
}

// ---------------------------------------------------------------------------
// Showcase scene AI compile coverage
//
// Every behavior shader referenced by demo/showcase/showcase.scene.xml must
// compile cleanly against the auto-prepended Nadir preamble. The showcase
// reuses shaders that live in demo/behaviors/; this test also opportunistically
// scans demo/showcase/behaviors/ if any showcase-specific shaders appear there.
// ---------------------------------------------------------------------------

namespace {

// Candidate search roots — tests may run from the repo root or from build/.
static std::vector<std::filesystem::path> candidate_roots_for(
    const std::string& relative) {
    return {
        std::filesystem::path(relative),
        std::filesystem::path("..") / relative,
        std::filesystem::path("../..") / relative,
        std::filesystem::path("T:/OdysseyEngine") / relative,
    };
}

static std::filesystem::path find_existing(const std::string& relative) {
    for (const auto& p : candidate_roots_for(relative)) {
        std::error_code ec;
        if (std::filesystem::exists(p, ec)) return p;
    }
    return {};
}

} // namespace

TEST(NadirShowcaseCompile, ShowcaseReferencedShadersCompile) {
    // Shaders named in demo/showcase/showcase.scene.xml <behavior shader="..."/>.
    // Resolved against both demo/behaviors/ (shared) and
    // demo/showcase/behaviors/ (showcase-specific overrides, if any).
    const std::vector<std::string> referenced_shaders = {
        "player_input.nadir",
        "enemy_pack_hunter.nadir",
        "enemy_ranged.nadir",
        "multi_arm_gunner.nadir",
        "civilian_fleeing.nadir",
    };

    const std::vector<std::string> search_dirs = {
        "demo/showcase/behaviors",  // preferred (overrides)
        "demo/behaviors",           // fallback (shared)
    };

    auto lib_dir = find_existing("behaviors/lib");
    ASSERT_FALSE(lib_dir.empty()) << "behaviors/lib include dir not found";

    size_t compiled = 0;
    for (const auto& name : referenced_shaders) {
        std::filesystem::path found;
        for (const auto& dir : search_dirs) {
            auto candidate = find_existing(dir + "/" + name);
            if (!candidate.empty()) { found = candidate; break; }
        }
        ASSERT_FALSE(found.empty())
            << "Showcase references missing shader: " << name;

        auto bytecode = odyssey::nadir::compile_nadir_file(found, lib_dir);
        ASSERT_TRUE(bytecode.is_ok())
            << name << ": read failed: " << bytecode.error();
        EXPECT_TRUE(bytecode.value().success)
            << name << ": " << bytecode.value().error_message;
        EXPECT_GT(bytecode.value().spirv.size(), 0u) << name;
        ++compiled;
    }
    EXPECT_EQ(compiled, referenced_shaders.size());
}

TEST(NadirShowcaseCompile, AllShadersInBothBehaviorDirsCompile) {
    // Scan both shader roots and compile every .nadir found. Catches
    // new shaders added under demo/showcase/behaviors/ without requiring
    // the test list above to be updated.
    auto lib_dir = find_existing("behaviors/lib");
    ASSERT_FALSE(lib_dir.empty());

    std::vector<std::filesystem::path> roots;
    for (const auto& rel : {"demo/behaviors", "demo/showcase/behaviors"}) {
        auto p = find_existing(rel);
        if (!p.empty()) roots.push_back(p);
    }
    ASSERT_FALSE(roots.empty()) << "No behavior shader roots found";

    size_t total = 0;
    for (const auto& root : roots) {
        auto files = odyssey::nadir::find_nadir_files(root);
        for (const auto& f : files) {
            auto bc = odyssey::nadir::compile_nadir_file(f, lib_dir);
            ASSERT_TRUE(bc.is_ok()) << f.string() << ": " << bc.error();
            EXPECT_TRUE(bc.value().success)
                << f.string() << ": " << bc.value().error_message;
            ++total;
        }
    }
    EXPECT_GT(total, 0u);
}
