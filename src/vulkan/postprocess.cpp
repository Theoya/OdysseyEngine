#include "vulkan/postprocess.h"

#include <shaderc/shaderc.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <fstream>
#include <vector>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Read a file from disk into a string
// ---------------------------------------------------------------------------

static std::string read_file_to_string(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    auto size = static_cast<size_t>(file.tellg());
    std::string buffer(size, '\0');
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
}

// ---------------------------------------------------------------------------
// PostProcessor::compile_shader
// ---------------------------------------------------------------------------

VkShaderModule PostProcessor::compile_shader(const std::filesystem::path& path,
                                             shaderc_shader_kind kind) {
    std::string source = read_file_to_string(path);
    if (source.empty()) {
        spdlog::error("Failed to read shader file: {}", path.string());
        return VK_NULL_HANDLE;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

    auto result = compiler.CompileGlslToSpv(source, kind, path.filename().string().c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        spdlog::error("Shader compilation failed for {}: {}",
                      path.string(), result.GetErrorMessage());
        return VK_NULL_HANDLE;
    }

    std::vector<uint32_t> spirv(result.cbegin(), result.cend());

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spirv.size() * sizeof(uint32_t);
    ci.pCode    = spirv.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult vk_result = vkCreateShaderModule(device_, &ci, nullptr, &module);
    if (vk_result != VK_SUCCESS) {
        spdlog::error("vkCreateShaderModule failed for {} with code {}",
                      path.string(), static_cast<int>(vk_result));
        return VK_NULL_HANDLE;
    }

    spdlog::info("Compiled shader: {}", path.filename().string());
    return module;
}

// ---------------------------------------------------------------------------
// PostProcessor::create_offscreen_target
// ---------------------------------------------------------------------------

Result<bool> PostProcessor::create_offscreen_target(VkExtent2D extent, VkFormat format) {
    // -- Color image --
    VkImageCreateInfo img_ci{};
    img_ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_ci.imageType     = VK_IMAGE_TYPE_2D;
    img_ci.format        = format;
    img_ci.extent        = {extent.width, extent.height, 1};
    img_ci.mipLevels     = 1;
    img_ci.arrayLayers   = 1;
    img_ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    img_ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    img_ci.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_ci{};
    alloc_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(allocator_, &img_ci, &alloc_ci,
                                     &offscreen_image_, &offscreen_alloc_, nullptr);
    if (result != VK_SUCCESS) {
        return Result<bool>::err(
            "vmaCreateImage (offscreen color) failed with code " +
            std::to_string(static_cast<int>(result)));
    }

    // -- Color image view --
    VkImageViewCreateInfo view_ci{};
    view_ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image    = offscreen_image_;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format   = format;
    view_ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.baseMipLevel   = 0;
    view_ci.subresourceRange.levelCount     = 1;
    view_ci.subresourceRange.baseArrayLayer = 0;
    view_ci.subresourceRange.layerCount     = 1;

    result = vkCreateImageView(device_, &view_ci, nullptr, &offscreen_view_);
    if (result != VK_SUCCESS) {
        return Result<bool>::err(
            "vkCreateImageView (offscreen color) failed with code " +
            std::to_string(static_cast<int>(result)));
    }

    // -- Sampler --
    VkSamplerCreateInfo sampler_ci{};
    sampler_ci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.magFilter    = VK_FILTER_LINEAR;
    sampler_ci.minFilter    = VK_FILTER_LINEAR;
    sampler_ci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.maxLod       = 1.0f;

    result = vkCreateSampler(device_, &sampler_ci, nullptr, &offscreen_sampler_);
    if (result != VK_SUCCESS) {
        return Result<bool>::err(
            "vkCreateSampler failed with code " +
            std::to_string(static_cast<int>(result)));
    }

    // -- Depth image --
    VkImageCreateInfo depth_ci{};
    depth_ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_ci.imageType     = VK_IMAGE_TYPE_2D;
    depth_ci.format        = VK_FORMAT_D32_SFLOAT;
    depth_ci.extent        = {extent.width, extent.height, 1};
    depth_ci.mipLevels     = 1;
    depth_ci.arrayLayers   = 1;
    depth_ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    depth_ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    depth_ci.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    depth_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = vmaCreateImage(allocator_, &depth_ci, &alloc_ci,
                            &offscreen_depth_, &offscreen_depth_alloc_, nullptr);
    if (result != VK_SUCCESS) {
        return Result<bool>::err(
            "vmaCreateImage (offscreen depth) failed with code " +
            std::to_string(static_cast<int>(result)));
    }

    // -- Depth image view --
    VkImageViewCreateInfo depth_view_ci{};
    depth_view_ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depth_view_ci.image    = offscreen_depth_;
    depth_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depth_view_ci.format   = VK_FORMAT_D32_SFLOAT;
    depth_view_ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_view_ci.subresourceRange.baseMipLevel   = 0;
    depth_view_ci.subresourceRange.levelCount     = 1;
    depth_view_ci.subresourceRange.baseArrayLayer = 0;
    depth_view_ci.subresourceRange.layerCount     = 1;

    result = vkCreateImageView(device_, &depth_view_ci, nullptr, &offscreen_depth_view_);
    if (result != VK_SUCCESS) {
        return Result<bool>::err(
            "vkCreateImageView (offscreen depth) failed with code " +
            std::to_string(static_cast<int>(result)));
    }

    spdlog::info("Offscreen render target created: {}x{}", extent.width, extent.height);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// PostProcessor::create_render_passes
// ---------------------------------------------------------------------------

Result<bool> PostProcessor::create_render_passes(VkFormat color_format) {
    // ---- Scene render pass (renders to offscreen) ----
    {
        // Color attachment: offscreen image
        VkAttachmentDescription color_att{};
        color_att.format         = color_format;
        color_att.samples        = VK_SAMPLE_COUNT_1_BIT;
        color_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        color_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        color_att.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Depth attachment
        VkAttachmentDescription depth_att{};
        depth_att.format         = VK_FORMAT_D32_SFLOAT;
        depth_att.samples        = VK_SAMPLE_COUNT_1_BIT;
        depth_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_att.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_att.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::array<VkAttachmentDescription, 2> attachments = {color_att, depth_att};

        VkAttachmentReference color_ref{};
        color_ref.attachment = 0;
        color_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_ref{};
        depth_ref.attachment = 1;
        depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &color_ref;
        subpass.pDepthStencilAttachment = &depth_ref;

        // Dependency: ensure the scene pass finishes writing color before
        // the post-process pass reads it as a sampled image.
        VkSubpassDependency dep{};
        dep.srcSubpass    = 0;
        dep.dstSubpass    = VK_SUBPASS_EXTERNAL;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rp_ci{};
        rp_ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_ci.attachmentCount = static_cast<uint32_t>(attachments.size());
        rp_ci.pAttachments    = attachments.data();
        rp_ci.subpassCount    = 1;
        rp_ci.pSubpasses      = &subpass;
        rp_ci.dependencyCount = 1;
        rp_ci.pDependencies   = &dep;

        VkResult result = vkCreateRenderPass(device_, &rp_ci, nullptr, &scene_render_pass_);
        if (result != VK_SUCCESS) {
            return Result<bool>::err(
                "vkCreateRenderPass (scene) failed with code " +
                std::to_string(static_cast<int>(result)));
        }
    }

    // ---- Post-process render pass (renders to swapchain) ----
    {
        VkAttachmentDescription color_att{};
        color_att.format         = color_format;
        color_att.samples        = VK_SAMPLE_COUNT_1_BIT;
        color_att.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        color_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        color_att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_ref{};
        color_ref.attachment = 0;
        color_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &color_ref;

        // Dependency: external -> subpass 0, wait for color attachment output.
        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rp_ci{};
        rp_ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_ci.attachmentCount = 1;
        rp_ci.pAttachments    = &color_att;
        rp_ci.subpassCount    = 1;
        rp_ci.pSubpasses      = &subpass;
        rp_ci.dependencyCount = 1;
        rp_ci.pDependencies   = &dep;

        VkResult result = vkCreateRenderPass(device_, &rp_ci, nullptr, &post_render_pass_);
        if (result != VK_SUCCESS) {
            return Result<bool>::err(
                "vkCreateRenderPass (post) failed with code " +
                std::to_string(static_cast<int>(result)));
        }
    }

    spdlog::info("Render passes created (scene + post-process)");
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// PostProcessor::create_framebuffers
// ---------------------------------------------------------------------------

Result<bool> PostProcessor::create_framebuffers() {
    // ---- Scene framebuffer (offscreen) ----
    {
        std::array<VkImageView, 2> attachments = {offscreen_view_, offscreen_depth_view_};

        VkFramebufferCreateInfo fb_ci{};
        fb_ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_ci.renderPass      = scene_render_pass_;
        fb_ci.attachmentCount = static_cast<uint32_t>(attachments.size());
        fb_ci.pAttachments    = attachments.data();
        fb_ci.width           = extent_.width;
        fb_ci.height          = extent_.height;
        fb_ci.layers          = 1;

        VkResult result = vkCreateFramebuffer(device_, &fb_ci, nullptr, &scene_framebuffer_);
        if (result != VK_SUCCESS) {
            return Result<bool>::err(
                "vkCreateFramebuffer (scene) failed with code " +
                std::to_string(static_cast<int>(result)));
        }
    }

    // ---- Post-process framebuffers (one per swapchain image) ----
    post_framebuffers_.resize(swapchain_views_.size());
    for (size_t i = 0; i < swapchain_views_.size(); ++i) {
        VkFramebufferCreateInfo fb_ci{};
        fb_ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_ci.renderPass      = post_render_pass_;
        fb_ci.attachmentCount = 1;
        fb_ci.pAttachments    = &swapchain_views_[i];
        fb_ci.width           = extent_.width;
        fb_ci.height          = extent_.height;
        fb_ci.layers          = 1;

        VkResult result = vkCreateFramebuffer(device_, &fb_ci, nullptr, &post_framebuffers_[i]);
        if (result != VK_SUCCESS) {
            // Clean up previously created post framebuffers.
            for (size_t j = 0; j < i; ++j) {
                vkDestroyFramebuffer(device_, post_framebuffers_[j], nullptr);
                post_framebuffers_[j] = VK_NULL_HANDLE;
            }
            return Result<bool>::err(
                "vkCreateFramebuffer (post #" + std::to_string(i) +
                ") failed with code " + std::to_string(static_cast<int>(result)));
        }
    }

    spdlog::info("Framebuffers created: 1 scene + {} post-process", swapchain_views_.size());
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// PostProcessor::create_descriptor
// ---------------------------------------------------------------------------

Result<bool> PostProcessor::create_descriptor() {
    // ---- Descriptor set layout ----
    VkDescriptorSetLayoutBinding binding{};
    binding.binding         = 0;
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layout_ci{};
    layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.bindingCount = 1;
    layout_ci.pBindings    = &binding;

    VkResult result = vkCreateDescriptorSetLayout(device_, &layout_ci, nullptr,
                                                  &descriptor_layout_);
    if (result != VK_SUCCESS) {
        return Result<bool>::err(
            "vkCreateDescriptorSetLayout (postprocess) failed with code " +
            std::to_string(static_cast<int>(result)));
    }

    // ---- Descriptor pool ----
    VkDescriptorPoolSize pool_size{};
    pool_size.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_ci{};
    pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.maxSets       = 1;
    pool_ci.poolSizeCount = 1;
    pool_ci.pPoolSizes    = &pool_size;

    result = vkCreateDescriptorPool(device_, &pool_ci, nullptr, &descriptor_pool_);
    if (result != VK_SUCCESS) {
        return Result<bool>::err(
            "vkCreateDescriptorPool (postprocess) failed with code " +
            std::to_string(static_cast<int>(result)));
    }

    // ---- Allocate descriptor set ----
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool     = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts        = &descriptor_layout_;

    result = vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set_);
    if (result != VK_SUCCESS) {
        return Result<bool>::err(
            "vkAllocateDescriptorSets (postprocess) failed with code " +
            std::to_string(static_cast<int>(result)));
    }

    // ---- Write descriptor: bind offscreen image + sampler ----
    VkDescriptorImageInfo img_info{};
    img_info.sampler     = offscreen_sampler_;
    img_info.imageView   = offscreen_view_;
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = descriptor_set_;
    write.dstBinding      = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &img_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    spdlog::info("Post-process descriptor set created and written");
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// PostProcessor::create_pipelines
// ---------------------------------------------------------------------------

Result<bool> PostProcessor::create_pipelines(const std::filesystem::path& shader_dir) {
    // ---- Compile shaders ----
    VkShaderModule vert_module = compile_shader(
        shader_dir / "crt_postprocess.vert", shaderc_vertex_shader);
    if (vert_module == VK_NULL_HANDLE) {
        return Result<bool>::err("Failed to compile crt_postprocess.vert");
    }

    VkShaderModule crt_frag_module = compile_shader(
        shader_dir / "crt_postprocess.frag", shaderc_fragment_shader);
    if (crt_frag_module == VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, vert_module, nullptr);
        return Result<bool>::err("Failed to compile crt_postprocess.frag");
    }

    VkShaderModule eva_frag_module = compile_shader(
        shader_dir / "eva_hud.frag", shaderc_fragment_shader);
    if (eva_frag_module == VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, vert_module, nullptr);
        vkDestroyShaderModule(device_, crt_frag_module, nullptr);
        return Result<bool>::err("Failed to compile eva_hud.frag");
    }

    // ---- Shared pipeline state ----

    // Vertex input: none (full-screen triangle via gl_VertexIndex)
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport and scissor (dynamic state)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_NONE;
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable  = VK_FALSE;
    depth_stencil.depthWriteEnable = VK_FALSE;

    std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates    = dynamic_states.data();

    // ---- CRT pipeline (opaque, no blending) ----
    {
        VkPipelineColorBlendAttachmentState blend_att{};
        blend_att.blendEnable    = VK_FALSE;
        blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blend_att;

        VkPushConstantRange push_range{};
        push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push_range.offset     = 0;
        push_range.size       = sizeof(CRTParams);

        VkPipelineLayoutCreateInfo layout_ci{};
        layout_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_ci.setLayoutCount         = 1;
        layout_ci.pSetLayouts            = &descriptor_layout_;
        layout_ci.pushConstantRangeCount = 1;
        layout_ci.pPushConstantRanges    = &push_range;

        VkResult result = vkCreatePipelineLayout(device_, &layout_ci, nullptr, &crt_layout_);
        if (result != VK_SUCCESS) {
            vkDestroyShaderModule(device_, vert_module, nullptr);
            vkDestroyShaderModule(device_, crt_frag_module, nullptr);
            vkDestroyShaderModule(device_, eva_frag_module, nullptr);
            return Result<bool>::err(
                "vkCreatePipelineLayout (CRT) failed with code " +
                std::to_string(static_cast<int>(result)));
        }

        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert_module;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = crt_frag_module;
        stages[1].pName  = "main";

        VkGraphicsPipelineCreateInfo pipe_ci{};
        pipe_ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipe_ci.stageCount          = static_cast<uint32_t>(stages.size());
        pipe_ci.pStages             = stages.data();
        pipe_ci.pVertexInputState   = &vertex_input;
        pipe_ci.pInputAssemblyState = &input_assembly;
        pipe_ci.pViewportState      = &viewport_state;
        pipe_ci.pRasterizationState = &rasterizer;
        pipe_ci.pMultisampleState   = &multisampling;
        pipe_ci.pDepthStencilState  = &depth_stencil;
        pipe_ci.pColorBlendState    = &blend;
        pipe_ci.pDynamicState       = &dynamic_state;
        pipe_ci.layout              = crt_layout_;
        pipe_ci.renderPass          = post_render_pass_;
        pipe_ci.subpass             = 0;

        result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipe_ci,
                                           nullptr, &crt_pipeline_);
        if (result != VK_SUCCESS) {
            vkDestroyShaderModule(device_, vert_module, nullptr);
            vkDestroyShaderModule(device_, crt_frag_module, nullptr);
            vkDestroyShaderModule(device_, eva_frag_module, nullptr);
            return Result<bool>::err(
                "vkCreateGraphicsPipelines (CRT) failed with code " +
                std::to_string(static_cast<int>(result)));
        }
    }

    // ---- EVA HUD pipeline (alpha blended on top) ----
    {
        VkPipelineColorBlendAttachmentState blend_att{};
        blend_att.blendEnable         = VK_TRUE;
        blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_att.colorBlendOp        = VK_BLEND_OP_ADD;
        blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend_att.alphaBlendOp        = VK_BLEND_OP_ADD;
        blend_att.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blend_att;

        VkPushConstantRange push_range{};
        push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push_range.offset     = 0;
        push_range.size       = sizeof(EvaHUDParams);

        VkPipelineLayoutCreateInfo layout_ci{};
        layout_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_ci.setLayoutCount         = 1;
        layout_ci.pSetLayouts            = &descriptor_layout_;
        layout_ci.pushConstantRangeCount = 1;
        layout_ci.pPushConstantRanges    = &push_range;

        VkResult result = vkCreatePipelineLayout(device_, &layout_ci, nullptr, &eva_layout_);
        if (result != VK_SUCCESS) {
            vkDestroyShaderModule(device_, vert_module, nullptr);
            vkDestroyShaderModule(device_, crt_frag_module, nullptr);
            vkDestroyShaderModule(device_, eva_frag_module, nullptr);
            return Result<bool>::err(
                "vkCreatePipelineLayout (EVA) failed with code " +
                std::to_string(static_cast<int>(result)));
        }

        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert_module;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = eva_frag_module;
        stages[1].pName  = "main";

        VkGraphicsPipelineCreateInfo pipe_ci{};
        pipe_ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipe_ci.stageCount          = static_cast<uint32_t>(stages.size());
        pipe_ci.pStages             = stages.data();
        pipe_ci.pVertexInputState   = &vertex_input;
        pipe_ci.pInputAssemblyState = &input_assembly;
        pipe_ci.pViewportState      = &viewport_state;
        pipe_ci.pRasterizationState = &rasterizer;
        pipe_ci.pMultisampleState   = &multisampling;
        pipe_ci.pDepthStencilState  = &depth_stencil;
        pipe_ci.pColorBlendState    = &blend;
        pipe_ci.pDynamicState       = &dynamic_state;
        pipe_ci.layout              = eva_layout_;
        pipe_ci.renderPass          = post_render_pass_;
        pipe_ci.subpass             = 0;

        result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipe_ci,
                                           nullptr, &eva_pipeline_);
        if (result != VK_SUCCESS) {
            vkDestroyShaderModule(device_, vert_module, nullptr);
            vkDestroyShaderModule(device_, crt_frag_module, nullptr);
            vkDestroyShaderModule(device_, eva_frag_module, nullptr);
            return Result<bool>::err(
                "vkCreateGraphicsPipelines (EVA) failed with code " +
                std::to_string(static_cast<int>(result)));
        }
    }

    // Shader modules can be destroyed after pipeline creation.
    vkDestroyShaderModule(device_, vert_module, nullptr);
    vkDestroyShaderModule(device_, crt_frag_module, nullptr);
    vkDestroyShaderModule(device_, eva_frag_module, nullptr);

    spdlog::info("Post-process pipelines created (CRT + EVA HUD)");
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// PostProcessor::initialize
// ---------------------------------------------------------------------------

Result<bool> PostProcessor::initialize(
    const DeviceContext& device_ctx,
    VkExtent2D extent,
    VkFormat color_format,
    VkCommandPool command_pool,
    const std::filesystem::path& shader_dir)
{
    device_    = device_ctx.device;
    allocator_ = device_ctx.allocator;
    extent_    = extent;

    // 1. Create offscreen render target (color + depth + sampler)
    auto result = create_offscreen_target(extent, color_format);
    if (result.is_err()) return result;

    // 2. Create render passes
    result = create_render_passes(color_format);
    if (result.is_err()) return result;

    // 3. Create descriptor set (binds offscreen image for post-process shaders)
    result = create_descriptor();
    if (result.is_err()) return result;

    // 4. Compile shaders and create graphics pipelines
    result = create_pipelines(shader_dir);
    if (result.is_err()) return result;

    // 5. Transition offscreen image from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL
    //    so the scene render pass can start cleanly.
    {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = command_pool;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        vkAllocateCommandBuffers(device_, &alloc_info, &cmd);

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin_info);

        // Transition offscreen color image
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = offscreen_image_;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Transition depth image
        VkImageMemoryBarrier depth_barrier{};
        depth_barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depth_barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_barrier.newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        depth_barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        depth_barrier.image                           = offscreen_depth_;
        depth_barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth_barrier.subresourceRange.baseMipLevel   = 0;
        depth_barrier.subresourceRange.levelCount     = 1;
        depth_barrier.subresourceRange.baseArrayLayer = 0;
        depth_barrier.subresourceRange.layerCount     = 1;
        depth_barrier.srcAccessMask                   = 0;
        depth_barrier.dstAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &depth_barrier);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;

        vkQueueSubmit(device_ctx.graphics_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(device_ctx.graphics_queue);

        vkFreeCommandBuffers(device_, command_pool, 1, &cmd);
    }

    spdlog::info("PostProcessor initialized ({}x{})", extent.width, extent.height);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// PostProcessor::set_swapchain_views
// ---------------------------------------------------------------------------

Result<bool> PostProcessor::set_swapchain_views(const std::vector<VkImageView>& views,
                                                VkExtent2D extent) {
    // Destroy old post framebuffers.
    for (auto fb : post_framebuffers_) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, fb, nullptr);
        }
    }
    post_framebuffers_.clear();

    // Destroy old scene framebuffer (it may need rebuilding if extent changed).
    if (scene_framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, scene_framebuffer_, nullptr);
        scene_framebuffer_ = VK_NULL_HANDLE;
    }

    swapchain_views_ = views;
    extent_          = extent;

    return create_framebuffers();
}

// ---------------------------------------------------------------------------
// PostProcessor::apply
// ---------------------------------------------------------------------------

void PostProcessor::apply(VkCommandBuffer cmd, uint32_t swapchain_image_index,
                          const CRTParams& crt, const EvaHUDParams& eva) {
    if (swapchain_image_index >= post_framebuffers_.size()) {
        spdlog::error("PostProcessor::apply: swapchain_image_index {} out of range ({})",
                      swapchain_image_index, post_framebuffers_.size());
        return;
    }

    // The scene render pass finalLayout already transitions offscreen_image_ to
    // SHADER_READ_ONLY_OPTIMAL, so no explicit barrier is needed here.

    // ---- Begin post-process render pass ----
    VkRenderPassBeginInfo rp_begin{};
    rp_begin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass        = post_render_pass_;
    rp_begin.framebuffer       = post_framebuffers_[swapchain_image_index];
    rp_begin.renderArea.offset = {0, 0};
    rp_begin.renderArea.extent = extent_;
    // loadOp is DONT_CARE, so no clear value needed, but Vulkan requires
    // the pointer to be valid if clearValueCount > 0. We set 0.
    rp_begin.clearValueCount   = 0;
    rp_begin.pClearValues      = nullptr;

    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(extent_.width);
    viewport.height   = static_cast<float>(extent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent_;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // ---- Draw 1: CRT effect (opaque full-screen triangle) ----
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, crt_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, crt_layout_,
                            0, 1, &descriptor_set_, 0, nullptr);
    vkCmdPushConstants(cmd, crt_layout_, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(CRTParams), &crt);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // ---- Draw 2: EVA HUD overlay (alpha-blended full-screen triangle) ----
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, eva_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, eva_layout_,
                            0, 1, &descriptor_set_, 0, nullptr);
    vkCmdPushConstants(cmd, eva_layout_, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(EvaHUDParams), &eva);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    // The post render pass finalLayout transitions the swapchain image to
    // PRESENT_SRC_KHR. The offscreen image is left in SHADER_READ_ONLY_OPTIMAL.
    // The scene render pass initialLayout is UNDEFINED with LOAD_OP_CLEAR,
    // so no transition back to COLOR_ATTACHMENT is needed -- the next scene
    // render pass will handle it via its initialLayout/loadOp.
}

// ---------------------------------------------------------------------------
// PostProcessor::shutdown
// ---------------------------------------------------------------------------

void PostProcessor::shutdown() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    // Wait for all GPU work to finish before destroying resources.
    vkDeviceWaitIdle(device_);

    // Pipelines
    if (crt_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, crt_pipeline_, nullptr);
        crt_pipeline_ = VK_NULL_HANDLE;
    }
    if (crt_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, crt_layout_, nullptr);
        crt_layout_ = VK_NULL_HANDLE;
    }
    if (eva_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, eva_pipeline_, nullptr);
        eva_pipeline_ = VK_NULL_HANDLE;
    }
    if (eva_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, eva_layout_, nullptr);
        eva_layout_ = VK_NULL_HANDLE;
    }

    // Descriptor resources
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
        descriptor_set_  = VK_NULL_HANDLE; // implicitly freed with pool
    }
    if (descriptor_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptor_layout_, nullptr);
        descriptor_layout_ = VK_NULL_HANDLE;
    }

    // Post-process framebuffers
    for (auto fb : post_framebuffers_) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, fb, nullptr);
        }
    }
    post_framebuffers_.clear();

    // Scene framebuffer
    if (scene_framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, scene_framebuffer_, nullptr);
        scene_framebuffer_ = VK_NULL_HANDLE;
    }

    // Render passes
    if (scene_render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, scene_render_pass_, nullptr);
        scene_render_pass_ = VK_NULL_HANDLE;
    }
    if (post_render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, post_render_pass_, nullptr);
        post_render_pass_ = VK_NULL_HANDLE;
    }

    // Offscreen resources
    if (offscreen_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, offscreen_sampler_, nullptr);
        offscreen_sampler_ = VK_NULL_HANDLE;
    }
    if (offscreen_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, offscreen_view_, nullptr);
        offscreen_view_ = VK_NULL_HANDLE;
    }
    if (offscreen_image_ != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator_, offscreen_image_, offscreen_alloc_);
        offscreen_image_ = VK_NULL_HANDLE;
        offscreen_alloc_ = VK_NULL_HANDLE;
    }
    if (offscreen_depth_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, offscreen_depth_view_, nullptr);
        offscreen_depth_view_ = VK_NULL_HANDLE;
    }
    if (offscreen_depth_ != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator_, offscreen_depth_, offscreen_depth_alloc_);
        offscreen_depth_ = VK_NULL_HANDLE;
        offscreen_depth_alloc_ = VK_NULL_HANDLE;
    }

    swapchain_views_.clear();
    device_    = VK_NULL_HANDLE;
    allocator_ = VK_NULL_HANDLE;

    spdlog::info("PostProcessor shut down");
}

} // namespace odyssey::vulkan
