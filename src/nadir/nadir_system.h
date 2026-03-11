#pragma once

#include "core/types.h"
#include "core/result.h"
#include "nadir/behavior_compiler.h"
#include "nadir/nadir_buffers.h"
#include "vulkan/device.h"

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

    // Transfer context — must be set before upload/readback calls
    void set_transfer_context(const vulkan::DeviceContext& device_ctx,
                              VkCommandPool transfer_pool);

    // Upload entity positions (vec4 per entity) to an archetype's transform buffer
    Result<bool> upload_transforms(const std::string& archetype,
                                   const vec4* data, uint32_t count);

    // Upload entity stats to an archetype's stats buffer
    Result<bool> upload_stats(const std::string& archetype,
                              const EntityStats* data, uint32_t count);

    // Upload world state to all archetypes (uses per-archetype entity_count)
    void upload_world_state_all(float world_time, float delta_time,
                                const vec3& player_pos, uint32_t frame_number);

    // Upload persist state to an archetype's persist buffer
    Result<bool> upload_persist(const std::string& archetype,
                                const void* data, VkDeviceSize size);

    // Readback behavior outputs from an archetype's output buffer
    Result<std::vector<BehaviorOutput>> readback_outputs(const std::string& archetype);

    // Set the actual entity count for an archetype (may differ from buffer capacity)
    void set_entity_count(const std::string& name, uint32_t count);

    // Accessors
    const Archetype* get_archetype(const std::string& name) const;
    const std::vector<Archetype>& get_archetypes() const { return archetypes_; }
    size_t archetype_count() const { return archetypes_.size(); }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    NadirConfig config_;

    // Transfer context for upload/readback
    const vulkan::DeviceContext* device_ctx_ = nullptr;
    VkCommandPool transfer_pool_ = VK_NULL_HANDLE;

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
