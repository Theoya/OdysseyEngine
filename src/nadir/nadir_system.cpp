#include "nadir/nadir_system.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>

namespace odyssey::nadir {

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------

std::string archetype_name_from_path(const std::filesystem::path& nadir_path) {
    return nadir_path.stem().string();
}

std::vector<std::filesystem::path> find_nadir_files(
    const std::filesystem::path& behavior_dir) {
    std::vector<std::filesystem::path> results;

    if (!std::filesystem::exists(behavior_dir) ||
        !std::filesystem::is_directory(behavior_dir)) {
        spdlog::warn("Behavior directory does not exist: {}", behavior_dir.string());
        return results;
    }

    for (const auto& entry : std::filesystem::directory_iterator(behavior_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".nadir") {
            results.push_back(entry.path());
        }
    }

    // Sort for deterministic ordering
    std::sort(results.begin(), results.end());
    return results;
}

// ---------------------------------------------------------------------------
// NadirSystem — initialization / shutdown
// ---------------------------------------------------------------------------

Result<bool> NadirSystem::initialize(VkDevice device,
                                     VmaAllocator allocator,
                                     VkDescriptorPool descriptor_pool,
                                     const NadirConfig& config) {
    device_ = device;
    allocator_ = allocator;
    descriptor_pool_ = descriptor_pool;
    config_ = config;

    spdlog::info("Nadir behavior system initialized");
    spdlog::info("  Behavior dir: {}", config_.behavior_dir.string());
    spdlog::info("  Library dir:  {}", config_.lib_dir.string());
    spdlog::info("  Hot reload:   {}", config_.hot_reload_enabled ? "enabled" : "disabled");
    spdlog::info("  Workgroup:    {}", config_.workgroup_size);

    return Result<bool>::ok(true);
}

void NadirSystem::shutdown() {
    for (auto& archetype : archetypes_) {
        // Destroy pipeline resources
        if (archetype.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, archetype.pipeline, nullptr);
        }
        if (archetype.pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, archetype.pipeline_layout, nullptr);
        }
        if (archetype.descriptor_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, archetype.descriptor_layout, nullptr);
        }
        if (archetype.shader_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, archetype.shader_module, nullptr);
        }
        // Descriptor sets are freed when the pool is destroyed

        // Destroy buffer set
        destroy_buffer_set(allocator_, archetype.buffers);
    }

    archetypes_.clear();
    archetype_index_.clear();
    next_id_ = 0;

    spdlog::info("Nadir behavior system shut down");
}

// ---------------------------------------------------------------------------
// Behavior loading
// ---------------------------------------------------------------------------

Result<bool> NadirSystem::load_behaviors() {
    auto nadir_files = find_nadir_files(config_.behavior_dir);

    if (nadir_files.empty()) {
        spdlog::warn("No .nadir files found in {}", config_.behavior_dir.string());
        return Result<bool>::ok(true); // Not an error — just nothing to load
    }

    spdlog::info("Found {} .nadir files", nadir_files.size());

    for (const auto& path : nadir_files) {
        std::string name = archetype_name_from_path(path);

        // Default entity count; will be resized when entities are actually spawned
        constexpr uint32_t default_entity_count = 1024;

        auto result = register_archetype(name, default_entity_count, path);
        if (!result.is_ok()) {
            spdlog::error("Failed to register archetype '{}': {}", name, result.error());
            return Result<bool>::err(
                "Failed to register archetype '" + name + "': " + result.error());
        }
    }

    spdlog::info("Loaded {} behavior archetypes", archetypes_.size());
    return Result<bool>::ok(true);
}

Result<bool> NadirSystem::register_archetype(const std::string& name,
                                              uint32_t entity_count,
                                              const std::filesystem::path& shader_path) {
    // Check for duplicate
    if (archetype_index_.contains(name)) {
        return Result<bool>::err("Archetype '" + name + "' is already registered");
    }

    Archetype archetype;
    archetype.name = name;
    archetype.id = next_id_++;
    archetype.entity_count = entity_count;
    archetype.shader_path = shader_path;

    // Record file modification time for hot-reload
    std::error_code ec;
    archetype.last_modified = std::filesystem::last_write_time(shader_path, ec);
    if (ec) {
        spdlog::warn("Could not read modification time for {}: {}",
                     shader_path.string(), ec.message());
    }

    // Compile shader and create pipeline
    auto compile_result = compile_and_create_pipeline(archetype);
    if (!compile_result.is_ok()) {
        return Result<bool>::err(compile_result.error());
    }

    // Create buffer set
    ArchetypeBufferDesc buffer_desc;
    buffer_desc.entity_count = entity_count;
    buffer_desc.needs_spatial_grid = true;
    buffer_desc.needs_debug_output = true;

    BufferSetLayout layout = compute_buffer_set_layout(buffer_desc);

    auto buffer_result = create_buffer_set(allocator_, layout);
    if (!buffer_result.is_ok()) {
        // Clean up pipeline resources before returning error
        if (archetype.pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device_, archetype.pipeline, nullptr);
        if (archetype.pipeline_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device_, archetype.pipeline_layout, nullptr);
        if (archetype.descriptor_layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device_, archetype.descriptor_layout, nullptr);
        if (archetype.shader_module != VK_NULL_HANDLE)
            vkDestroyShaderModule(device_, archetype.shader_module, nullptr);

        return Result<bool>::err("Buffer creation failed: " + buffer_result.error());
    }
    archetype.buffers = std::move(buffer_result.value());

    // Allocate and update descriptor set
    auto desc_result = allocate_and_update_descriptor_set(
        archetype.descriptor_layout, archetype.buffers, layout);
    if (!desc_result.is_ok()) {
        destroy_buffer_set(allocator_, archetype.buffers);
        vkDestroyPipeline(device_, archetype.pipeline, nullptr);
        vkDestroyPipelineLayout(device_, archetype.pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(device_, archetype.descriptor_layout, nullptr);
        vkDestroyShaderModule(device_, archetype.shader_module, nullptr);
        return Result<bool>::err("Descriptor set creation failed: " + desc_result.error());
    }
    archetype.descriptor_set = desc_result.value();

    // Store
    size_t index = archetypes_.size();
    archetypes_.push_back(std::move(archetype));
    archetype_index_[name] = index;

    spdlog::info("Registered archetype '{}' (id={}, entities={})",
                 name, archetypes_[index].id, entity_count);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Hot-reload
// ---------------------------------------------------------------------------

std::vector<std::string> NadirSystem::check_hot_reload() {
    std::vector<std::string> reloaded;

    if (!config_.hot_reload_enabled) {
        return reloaded;
    }

    for (auto& archetype : archetypes_) {
        std::error_code ec;
        auto current_time = std::filesystem::last_write_time(archetype.shader_path, ec);
        if (ec) {
            continue; // File may have been deleted; skip
        }

        if (current_time <= archetype.last_modified) {
            continue; // Not modified
        }

        spdlog::info("Hot-reload: {} has changed, recompiling...",
                     archetype.shader_path.string());

        // Save old pipeline handles so we can destroy them after creating new ones
        VkPipeline old_pipeline = archetype.pipeline;
        VkPipelineLayout old_layout = archetype.pipeline_layout;
        VkShaderModule old_module = archetype.shader_module;
        // Keep old descriptor_layout — it doesn't change between reloads

        // Reset handles before compiling (compile_and_create_pipeline sets them)
        archetype.pipeline = VK_NULL_HANDLE;
        archetype.pipeline_layout = VK_NULL_HANDLE;
        archetype.shader_module = VK_NULL_HANDLE;

        auto result = compile_and_create_pipeline(archetype);
        if (result.is_ok()) {
            // Success — destroy old pipeline resources
            if (old_pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(device_, old_pipeline, nullptr);
            if (old_layout != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(device_, old_layout, nullptr);
            if (old_module != VK_NULL_HANDLE)
                vkDestroyShaderModule(device_, old_module, nullptr);

            archetype.last_modified = current_time;
            reloaded.push_back(archetype.name);

            spdlog::info("Hot-reload: '{}' recompiled successfully", archetype.name);
        } else {
            // Failure — restore old pipeline handles
            archetype.pipeline = old_pipeline;
            archetype.pipeline_layout = old_layout;
            archetype.shader_module = old_module;

            spdlog::error("Hot-reload: '{}' compilation failed: {}",
                          archetype.name, result.error());
        }
    }

    return reloaded;
}

// ---------------------------------------------------------------------------
// Command recording
// ---------------------------------------------------------------------------

void NadirSystem::record_dispatches(VkCommandBuffer cmd) const {
    for (size_t i = 0; i < archetypes_.size(); ++i) {
        const auto& archetype = archetypes_[i];

        if (archetype.pipeline == VK_NULL_HANDLE ||
            archetype.descriptor_set == VK_NULL_HANDLE ||
            archetype.entity_count == 0) {
            continue;
        }

        // Bind compute pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, archetype.pipeline);

        // Bind descriptor set
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            archetype.pipeline_layout,
            0, 1, &archetype.descriptor_set,
            0, nullptr
        );

        // Compute dispatch dimensions
        uint32_t group_count =
            (archetype.entity_count + config_.workgroup_size - 1) / config_.workgroup_size;

        vkCmdDispatch(cmd, group_count, 1, 1);

        // Insert a memory barrier between archetypes to ensure writes are visible
        // to subsequent dispatches (if buffers are shared)
        if (i + 1 < archetypes_.size()) {
            VkMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1, &barrier,
                0, nullptr,
                0, nullptr
            );
        }
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const Archetype* NadirSystem::get_archetype(const std::string& name) const {
    auto it = archetype_index_.find(name);
    if (it == archetype_index_.end()) {
        return nullptr;
    }
    return &archetypes_[it->second];
}

// ---------------------------------------------------------------------------
// Internal: compile shader and create compute pipeline
// ---------------------------------------------------------------------------

Result<bool> NadirSystem::compile_and_create_pipeline(Archetype& archetype) {
    // Compile .nadir file
    auto compile_result = compile_nadir_file(archetype.shader_path, config_.lib_dir);
    if (!compile_result.is_ok()) {
        return Result<bool>::err(compile_result.error());
    }

    const ShaderBytecode& bytecode = compile_result.value();

    // Create shader module
    VkShaderModuleCreateInfo module_info{};
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.codeSize = bytecode.spirv.size() * sizeof(uint32_t);
    module_info.pCode = bytecode.spirv.data();

    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkResult vk_result = vkCreateShaderModule(device_, &module_info, nullptr, &shader_module);
    if (vk_result != VK_SUCCESS) {
        return Result<bool>::err(
            "vkCreateShaderModule failed with VkResult " + std::to_string(vk_result));
    }
    archetype.shader_module = shader_module;

    // Create descriptor set layout (if not already created)
    if (archetype.descriptor_layout == VK_NULL_HANDLE) {
        auto layout_result = create_descriptor_layout();
        if (!layout_result.is_ok()) {
            vkDestroyShaderModule(device_, shader_module, nullptr);
            archetype.shader_module = VK_NULL_HANDLE;
            return Result<bool>::err(layout_result.error());
        }
        archetype.descriptor_layout = layout_result.value();
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &archetype.descriptor_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    vk_result = vkCreatePipelineLayout(device_, &pipeline_layout_info, nullptr,
                                       &pipeline_layout);
    if (vk_result != VK_SUCCESS) {
        vkDestroyShaderModule(device_, shader_module, nullptr);
        archetype.shader_module = VK_NULL_HANDLE;
        return Result<bool>::err(
            "vkCreatePipelineLayout failed with VkResult " + std::to_string(vk_result));
    }
    archetype.pipeline_layout = pipeline_layout;

    // Create compute pipeline
    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_info.stage.module = shader_module;
    pipeline_info.stage.pName = "main";

    VkPipeline pipeline = VK_NULL_HANDLE;
    vk_result = vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info,
                                         nullptr, &pipeline);
    if (vk_result != VK_SUCCESS) {
        vkDestroyPipelineLayout(device_, pipeline_layout, nullptr);
        vkDestroyShaderModule(device_, shader_module, nullptr);
        archetype.pipeline_layout = VK_NULL_HANDLE;
        archetype.shader_module = VK_NULL_HANDLE;
        return Result<bool>::err(
            "vkCreateComputePipelines failed with VkResult " + std::to_string(vk_result));
    }
    archetype.pipeline = pipeline;

    spdlog::info("Created compute pipeline for archetype '{}' ({} SPIR-V words)",
                 archetype.name, bytecode.spirv.size());

    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Internal: create descriptor set layout for the 7-binding scheme
// ---------------------------------------------------------------------------

Result<VkDescriptorSetLayout> NadirSystem::create_descriptor_layout() {
    // 7 bindings: 0-2 = storage (read), 3 = uniform, 4 = storage (rw),
    //             5-6 = storage (write)
    std::array<VkDescriptorSetLayoutBinding, 7> bindings{};

    // Binding 0: TransformBuffer (storage, read-only in shader)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: StatsBuffer
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: SpatialBuffer
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: WorldStateUBO (uniform buffer)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 4: PersistBuffer (storage, read/write)
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 5: OutputBuffer (storage, write-only in shader)
    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 6: DebugBuffer (storage, write-only in shader)
    bindings[6].binding = 6;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult vk_result = vkCreateDescriptorSetLayout(device_, &layout_info,
                                                     nullptr, &layout);
    if (vk_result != VK_SUCCESS) {
        return Result<VkDescriptorSetLayout>::err(
            "vkCreateDescriptorSetLayout failed with VkResult " +
            std::to_string(vk_result));
    }

    return Result<VkDescriptorSetLayout>::ok(layout);
}

// ---------------------------------------------------------------------------
// Internal: allocate descriptor set and write buffer descriptors
// ---------------------------------------------------------------------------

Result<VkDescriptorSet> NadirSystem::allocate_and_update_descriptor_set(
    VkDescriptorSetLayout layout,
    const BufferSet& buffers,
    const BufferSetLayout& buffer_layout)
{
    // Allocate descriptor set from pool
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VkResult vk_result = vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set);
    if (vk_result != VK_SUCCESS) {
        return Result<VkDescriptorSet>::err(
            "vkAllocateDescriptorSets failed with VkResult " +
            std::to_string(vk_result));
    }

    // Prepare buffer info for each binding
    std::array<VkDescriptorBufferInfo, 7> buffer_infos{};

    buffer_infos[0] = {buffers.transforms, 0, buffer_layout.transform_size};
    buffer_infos[1] = {buffers.stats, 0, buffer_layout.stats_size};
    buffer_infos[2] = {buffers.spatial, 0,
                       buffer_layout.spatial_size > 0
                           ? buffer_layout.spatial_size
                           : VK_WHOLE_SIZE};
    buffer_infos[3] = {buffers.world_state, 0, buffer_layout.world_state_size};
    buffer_infos[4] = {buffers.persist_state, 0, buffer_layout.persist_size};
    buffer_infos[5] = {buffers.output, 0, buffer_layout.output_size};
    buffer_infos[6] = {buffers.debug_output, 0,
                       buffer_layout.debug_size > 0
                           ? buffer_layout.debug_size
                           : VK_WHOLE_SIZE};

    // Write descriptor set
    std::array<VkWriteDescriptorSet, 7> writes{};
    for (uint32_t i = 0; i < 7; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptor_set;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = (i == 3)
            ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
            : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &buffer_infos[i];
    }

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);

    return Result<VkDescriptorSet>::ok(descriptor_set);
}

} // namespace odyssey::nadir
