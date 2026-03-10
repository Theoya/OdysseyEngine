#include "vulkan/compute_pipeline.h"

#include <spdlog/spdlog.h>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Impure: create compute pipeline
// ---------------------------------------------------------------------------

Result<ComputePipelineContext> create_compute_pipeline(
    VkDevice device,
    const ComputePipelineConfig& config)
{
    ComputePipelineContext ctx;

    // 1. Create shader module from SPIR-V bytecode.
    VkShaderModuleCreateInfo shader_ci{};
    shader_ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_ci.codeSize = config.spirv_code.size() * sizeof(uint32_t);
    shader_ci.pCode    = config.spirv_code.data();

    VkResult result = vkCreateShaderModule(device, &shader_ci, nullptr, &ctx.shader_module);
    if (result != VK_SUCCESS) {
        return Result<ComputePipelineContext>::err(
            "vkCreateShaderModule failed with code " + std::to_string(static_cast<int>(result)));
    }

    // 2. Create descriptor set layout from binding descriptions.
    std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
    layout_bindings.reserve(config.bindings.size());
    for (const auto& desc : config.bindings) {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = desc.binding;
        b.descriptorType  = desc.type;
        b.descriptorCount = 1;
        b.stageFlags      = desc.stage;
        layout_bindings.push_back(b);
    }

    VkDescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = static_cast<uint32_t>(layout_bindings.size());
    dsl_ci.pBindings    = layout_bindings.data();

    result = vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &ctx.descriptor_set_layout);
    if (result != VK_SUCCESS) {
        vkDestroyShaderModule(device, ctx.shader_module, nullptr);
        ctx.shader_module = VK_NULL_HANDLE;
        return Result<ComputePipelineContext>::err(
            "vkCreateDescriptorSetLayout failed with code " + std::to_string(static_cast<int>(result)));
    }

    // 3. Create pipeline layout (with optional push constants).
    VkPipelineLayoutCreateInfo pl_ci{};
    pl_ci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount = 1;
    pl_ci.pSetLayouts    = &ctx.descriptor_set_layout;

    VkPushConstantRange push_range{};
    if (config.push_constant_size > 0) {
        push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_range.offset     = 0;
        push_range.size       = config.push_constant_size;
        pl_ci.pushConstantRangeCount = 1;
        pl_ci.pPushConstantRanges    = &push_range;
    }

    result = vkCreatePipelineLayout(device, &pl_ci, nullptr, &ctx.pipeline_layout);
    if (result != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, ctx.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, ctx.shader_module, nullptr);
        ctx.descriptor_set_layout = VK_NULL_HANDLE;
        ctx.shader_module         = VK_NULL_HANDLE;
        return Result<ComputePipelineContext>::err(
            "vkCreatePipelineLayout failed with code " + std::to_string(static_cast<int>(result)));
    }

    // 4. Create compute pipeline.
    VkComputePipelineCreateInfo pipe_ci{};
    pipe_ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipe_ci.layout = ctx.pipeline_layout;
    pipe_ci.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipe_ci.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pipe_ci.stage.module = ctx.shader_module;
    pipe_ci.stage.pName  = "main";

    result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &ctx.pipeline);
    if (result != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, ctx.pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(device, ctx.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, ctx.shader_module, nullptr);
        ctx.pipeline_layout       = VK_NULL_HANDLE;
        ctx.descriptor_set_layout = VK_NULL_HANDLE;
        ctx.shader_module         = VK_NULL_HANDLE;
        return Result<ComputePipelineContext>::err(
            "vkCreateComputePipelines failed with code " + std::to_string(static_cast<int>(result)));
    }

    spdlog::info("Compute pipeline created ({} bindings, {} bytes push constants)",
                 config.bindings.size(), config.push_constant_size);
    return Result<ComputePipelineContext>::ok(std::move(ctx));
}

void destroy_compute_pipeline(VkDevice device, ComputePipelineContext& ctx) {
    if (ctx.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, ctx.pipeline, nullptr);
        ctx.pipeline = VK_NULL_HANDLE;
    }
    if (ctx.pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, ctx.pipeline_layout, nullptr);
        ctx.pipeline_layout = VK_NULL_HANDLE;
    }
    if (ctx.descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, ctx.descriptor_set_layout, nullptr);
        ctx.descriptor_set_layout = VK_NULL_HANDLE;
    }
    if (ctx.shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, ctx.shader_module, nullptr);
        ctx.shader_module = VK_NULL_HANDLE;
    }
    spdlog::info("Compute pipeline destroyed");
}

// ---------------------------------------------------------------------------
// Pure: compute descriptor pool config
// ---------------------------------------------------------------------------

DescriptorPoolConfig compute_descriptor_pool_config(
    uint32_t archetype_count,
    uint32_t buffers_per_archetype)
{
    DescriptorPoolConfig config;

    // One descriptor set per archetype.
    config.max_sets = archetype_count;

    // Total storage buffer descriptors needed.
    uint32_t total_storage_buffers = archetype_count * buffers_per_archetype;

    if (total_storage_buffers > 0) {
        VkDescriptorPoolSize pool_size{};
        pool_size.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = total_storage_buffers;
        config.pool_sizes.push_back(pool_size);
    }

    // Also reserve some uniform buffer descriptors (e.g., for WorldState).
    VkDescriptorPoolSize uniform_size{};
    uniform_size.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform_size.descriptorCount = archetype_count; // one per set
    config.pool_sizes.push_back(uniform_size);

    return config;
}

// ---------------------------------------------------------------------------
// Impure: descriptor pool create / destroy
// ---------------------------------------------------------------------------

Result<VkDescriptorPool> create_descriptor_pool(
    VkDevice device,
    const DescriptorPoolConfig& config)
{
    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.maxSets       = config.max_sets;
    ci.poolSizeCount = static_cast<uint32_t>(config.pool_sizes.size());
    ci.pPoolSizes    = config.pool_sizes.data();

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(device, &ci, nullptr, &pool);
    if (result != VK_SUCCESS) {
        return Result<VkDescriptorPool>::err(
            "vkCreateDescriptorPool failed with code " + std::to_string(static_cast<int>(result)));
    }

    spdlog::info("Descriptor pool created (max_sets: {})", config.max_sets);
    return Result<VkDescriptorPool>::ok(pool);
}

void destroy_descriptor_pool(VkDevice device, VkDescriptorPool pool) {
    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, pool, nullptr);
        spdlog::info("Descriptor pool destroyed");
    }
}

// ---------------------------------------------------------------------------
// Impure: allocate / update descriptor sets
// ---------------------------------------------------------------------------

Result<VkDescriptorSet> allocate_descriptor_set(
    VkDevice device,
    VkDescriptorPool pool,
    VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool     = pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts        = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult result = vkAllocateDescriptorSets(device, &alloc_info, &set);
    if (result != VK_SUCCESS) {
        return Result<VkDescriptorSet>::err(
            "vkAllocateDescriptorSets failed with code " + std::to_string(static_cast<int>(result)));
    }

    return Result<VkDescriptorSet>::ok(set);
}

void update_descriptor_set(
    VkDevice device,
    VkDescriptorSet set,
    const std::vector<std::pair<uint32_t, VkBuffer>>& buffer_bindings,
    const std::vector<VkDeviceSize>& buffer_sizes)
{
    std::vector<VkDescriptorBufferInfo> buffer_infos(buffer_bindings.size());
    std::vector<VkWriteDescriptorSet> writes(buffer_bindings.size());

    for (size_t i = 0; i < buffer_bindings.size(); ++i) {
        buffer_infos[i].buffer = buffer_bindings[i].second;
        buffer_infos[i].offset = 0;
        buffer_infos[i].range  = (i < buffer_sizes.size()) ? buffer_sizes[i] : VK_WHOLE_SIZE;

        writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext           = nullptr;
        writes[i].dstSet          = set;
        writes[i].dstBinding      = buffer_bindings[i].first;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo     = &buffer_infos[i];
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

} // namespace odyssey::vulkan
