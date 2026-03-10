#pragma once

#include "core/types.h"
#include "core/result.h"
#include "nadir/behavior_compiler.h"
#include "nadir/nadir_buffers.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace odyssey::nadir {

// Registered archetype with its behavior shader and buffers
struct Archetype {
    std::string name;
    ArchetypeID id = 0;
    uint32_t entity_count = 0;

    // GPU resources
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VkShaderModule shader_module = VK_NULL_HANDLE;

    BufferSet buffers;

    // Source tracking for hot-reload
    std::filesystem::path shader_path;
    std::filesystem::file_time_type last_modified;
};

// Nadir system configuration
struct NadirConfig {
    std::filesystem::path behavior_dir = "behaviors/shaders";
    std::filesystem::path lib_dir = "behaviors/lib";
    bool hot_reload_enabled = true;
    uint32_t max_agents = 100000;
    uint32_t workgroup_size = 256;
};

// Pure: extract archetype name from .nadir filename
// e.g. "enemy_pack_hunter.nadir" -> "enemy_pack_hunter"
std::string archetype_name_from_path(const std::filesystem::path& nadir_path);

// Pure-ish: find all .nadir files in a directory (filesystem scan)
std::vector<std::filesystem::path> find_nadir_files(const std::filesystem::path& behavior_dir);

class NadirSystem {
public:
    NadirSystem() = default;
    ~NadirSystem() = default;

    // Non-copyable, movable
    NadirSystem(const NadirSystem&) = delete;
    NadirSystem& operator=(const NadirSystem&) = delete;
    NadirSystem(NadirSystem&&) = default;
    NadirSystem& operator=(NadirSystem&&) = default;

    // Initialize with Vulkan device context
    Result<bool> initialize(
        VkDevice device,
        VmaAllocator allocator,
        VkDescriptorPool descriptor_pool,
        const NadirConfig& config
    );

    // Shut down and release all GPU resources
    void shutdown();

    // Scan behavior directory and compile/register all .nadir files
    Result<bool> load_behaviors();

    // Register a single archetype with entity count
    Result<bool> register_archetype(
        const std::string& name,
        uint32_t entity_count,
        const std::filesystem::path& shader_path
    );

    // Hot-reload: check for changed files and recompile
    // Returns names of archetypes that were reloaded
    std::vector<std::string> check_hot_reload();

    // Record all behavior dispatches into a command buffer
    void record_dispatches(VkCommandBuffer cmd) const;

    // Accessors
    const Archetype* get_archetype(const std::string& name) const;
    const std::vector<Archetype>& get_archetypes() const { return archetypes_; }
    size_t archetype_count() const { return archetypes_.size(); }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    NadirConfig config_;

    std::vector<Archetype> archetypes_;
    std::unordered_map<std::string, size_t> archetype_index_;
    ArchetypeID next_id_ = 0;

    // Internal: compile shader and create compute pipeline for an archetype
    Result<bool> compile_and_create_pipeline(Archetype& archetype);

    // Internal: create descriptor set layout for the 7-buffer binding scheme
    Result<VkDescriptorSetLayout> create_descriptor_layout();

    // Internal: allocate and update descriptor set for an archetype
    Result<VkDescriptorSet> allocate_and_update_descriptor_set(
        VkDescriptorSetLayout layout,
        const BufferSet& buffers,
        const BufferSetLayout& buffer_layout
    );
};

} // namespace odyssey::nadir
