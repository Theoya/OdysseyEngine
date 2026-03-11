#include "vulkan/renderer.h"
#include "vulkan/command.h"

#include <shaderc/shaderc.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace odyssey::vulkan {

// ---------------------------------------------------------------------------
// Shader compilation helpers (local to this translation unit)
// ---------------------------------------------------------------------------

namespace {

/// Read the entire contents of a text file into a string.
std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open shader file: {}", path);
        return {};
    }
    auto size = file.tellg();
    file.seekg(0);
    std::string contents(static_cast<size_t>(size), '\0');
    file.read(contents.data(), size);
    return contents;
}

/// Compile GLSL source to SPIR-V using shaderc.
Result<std::vector<uint32_t>> compile_glsl(
    const std::string& source,
    const std::string& filename,
    shaderc_shader_kind kind)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    auto result = compiler.CompileGlslToSpv(source, kind, filename.c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        return Result<std::vector<uint32_t>>::err(
            "Shader compilation failed (" + filename + "): " + result.GetErrorMessage());
    }

    return Result<std::vector<uint32_t>>::ok({result.cbegin(), result.cend()});
}

/// Create a VkShaderModule from SPIR-V bytecode.
Result<VkShaderModule> create_shader_module(VkDevice device, const std::vector<uint32_t>& spirv) {
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spirv.size() * sizeof(uint32_t);
    ci.pCode    = spirv.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult vr = vkCreateShaderModule(device, &ci, nullptr, &module);
    if (vr != VK_SUCCESS) {
        return Result<VkShaderModule>::err(
            "vkCreateShaderModule failed with code " + std::to_string(static_cast<int>(vr)));
    }
    return Result<VkShaderModule>::ok(module);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

Result<bool> Renderer::initialize(
    const DeviceContext& device_ctx,
    const SwapchainContext& swapchain_ctx,
    VkCommandPool command_pool)
{
    device_    = device_ctx.device;
    allocator_ = device_ctx.allocator;
    extent_    = swapchain_ctx.extent;

    // Create render pass
    auto rp_result = create_render_pass(swapchain_ctx.format);
    if (rp_result.is_err()) return Result<bool>::err(rp_result.error());

    // Create depth resources
    auto depth_result = create_depth_resources(swapchain_ctx.extent);
    if (depth_result.is_err()) return Result<bool>::err(depth_result.error());

    // Create framebuffers
    auto fb_result = create_framebuffers(swapchain_ctx);
    if (fb_result.is_err()) return Result<bool>::err(fb_result.error());

    // Create graphics pipeline (compiles shaders)
    auto pipe_result = create_pipeline();
    if (pipe_result.is_err()) return Result<bool>::err(pipe_result.error());

    // Create primitive meshes
    auto mesh_result = create_primitive_meshes(command_pool, device_ctx.graphics_queue);
    if (mesh_result.is_err()) return Result<bool>::err(mesh_result.error());

    spdlog::info("Renderer initialized ({}x{}, {} framebuffers)",
                 extent_.width, extent_.height, framebuffers_.size());
    return Result<bool>::ok(true);
}

void Renderer::shutdown() {
    if (device_ == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(device_);

    destroy_mesh(box_mesh_);
    destroy_mesh(sphere_mesh_);
    destroy_mesh(ground_mesh_);
    destroy_mesh(cylinder_mesh_);

    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }

    for (auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, nullptr);
    }
    framebuffers_.clear();

    if (depth_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depth_view_, nullptr);
        depth_view_ = VK_NULL_HANDLE;
    }
    if (depth_image_ != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator_, depth_image_, depth_alloc_);
        depth_image_ = VK_NULL_HANDLE;
        depth_alloc_ = VK_NULL_HANDLE;
    }

    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }

    spdlog::info("Renderer destroyed");
    device_ = VK_NULL_HANDLE;
}

Result<VkCommandBuffer> Renderer::begin_frame(uint32_t image_index, VkCommandBuffer cmd) {
    if (image_index >= framebuffers_.size()) {
        return Result<VkCommandBuffer>::err(
            "image_index " + std::to_string(image_index) + " out of range ("
            + std::to_string(framebuffers_.size()) + " framebuffers)");
    }

    active_cmd_ = cmd;

    // Begin render pass
    VkRenderPassBeginInfo rp_begin{};
    rp_begin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass        = render_pass_;
    rp_begin.framebuffer       = framebuffers_[image_index];
    rp_begin.renderArea.offset = {0, 0};
    rp_begin.renderArea.extent = extent_;

    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color        = {{0.1f, 0.1f, 0.12f, 1.0f}}; // dark gray background
    clear_values[1].depthStencil = {1.0f, 0};

    rp_begin.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rp_begin.pClearValues    = clear_values.data();

    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    // Set dynamic viewport and scissor
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

    return Result<VkCommandBuffer>::ok(cmd);
}

Result<VkCommandBuffer> Renderer::begin_frame_offscreen(VkRenderPass render_pass,
                                                         VkFramebuffer framebuffer,
                                                         VkExtent2D extent,
                                                         VkCommandBuffer cmd) {
    active_cmd_ = cmd;

    VkRenderPassBeginInfo rp_begin{};
    rp_begin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass        = render_pass;
    rp_begin.framebuffer       = framebuffer;
    rp_begin.renderArea.offset = {0, 0};
    rp_begin.renderArea.extent = extent;

    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color        = {{0.1f, 0.1f, 0.12f, 1.0f}};
    clear_values[1].depthStencil = {1.0f, 0};

    rp_begin.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rp_begin.pClearValues    = clear_values.data();

    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(extent.width);
    viewport.height   = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    return Result<VkCommandBuffer>::ok(cmd);
}

void Renderer::draw(const mat4& mvp, const vec4& color, PrimitiveType mesh_type) {
    if (active_cmd_ == VK_NULL_HANDLE) return;

    // Select the mesh
    const PrimitiveMesh* mesh = nullptr;
    switch (mesh_type) {
        case PrimitiveType::BOX:          mesh = &box_mesh_;    break;
        case PrimitiveType::SPHERE:       mesh = &sphere_mesh_; break;
        case PrimitiveType::GROUND_PLANE: mesh = &ground_mesh_; break;
        case PrimitiveType::CYLINDER:     mesh = &cylinder_mesh_; break;
    }
    if (!mesh || mesh->vertex_buffer == VK_NULL_HANDLE) return;

    // Push constants
    BasicPushConstants pc{};
    pc.mvp   = mvp;
    pc.color = color;
    vkCmdPushConstants(active_cmd_, pipeline_layout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(BasicPushConstants), &pc);

    // Bind vertex buffer
    VkBuffer vertex_buffers[] = {mesh->vertex_buffer};
    VkDeviceSize offsets[]    = {0};
    vkCmdBindVertexBuffers(active_cmd_, 0, 1, vertex_buffers, offsets);

    // Bind index buffer and draw
    vkCmdBindIndexBuffer(active_cmd_, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(active_cmd_, mesh->index_count, 1, 0, 0, 0);
}

void Renderer::end_frame(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
    active_cmd_ = VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// Public: recreate_for_resize
// ---------------------------------------------------------------------------

Result<bool> Renderer::recreate_for_resize(VkExtent2D new_extent,
                                            const std::vector<VkImageView>& swapchain_views) {
    // Destroy old depth resources
    if (depth_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depth_view_, nullptr);
        depth_view_ = VK_NULL_HANDLE;
    }
    if (depth_image_ != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator_, depth_image_, depth_alloc_);
        depth_image_ = VK_NULL_HANDLE;
        depth_alloc_ = VK_NULL_HANDLE;
    }

    // Destroy old framebuffers
    for (auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, nullptr);
    }
    framebuffers_.clear();

    extent_ = new_extent;

    // Recreate depth buffer
    auto depth_result = create_depth_resources(new_extent);
    if (depth_result.is_err()) return depth_result;

    // Recreate framebuffers
    framebuffers_.resize(swapchain_views.size());
    for (size_t i = 0; i < swapchain_views.size(); ++i) {
        std::array<VkImageView, 2> attachments = {
            swapchain_views[i],
            depth_view_
        };

        VkFramebufferCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass      = render_pass_;
        ci.attachmentCount = static_cast<uint32_t>(attachments.size());
        ci.pAttachments    = attachments.data();
        ci.width           = new_extent.width;
        ci.height          = new_extent.height;
        ci.layers          = 1;

        VkResult vr = vkCreateFramebuffer(device_, &ci, nullptr, &framebuffers_[i]);
        if (vr != VK_SUCCESS) {
            return Result<bool>::err(
                "vkCreateFramebuffer failed during resize at index " + std::to_string(i));
        }
    }

    spdlog::info("Renderer resized to {}x{}", new_extent.width, new_extent.height);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Private: render pass
// ---------------------------------------------------------------------------

Result<bool> Renderer::create_render_pass(VkFormat color_format) {
    // Color attachment (swapchain image)
    VkAttachmentDescription color_attachment{};
    color_attachment.format         = color_format;
    color_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    VkAttachmentDescription depth_attachment{};
    depth_attachment.format         = VK_FORMAT_D32_SFLOAT;
    depth_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

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

    // Subpass dependency: ensure the color attachment is ready before we write to it
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                             | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {color_attachment, depth_attachment};

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = static_cast<uint32_t>(attachments.size());
    ci.pAttachments    = attachments.data();
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dependency;

    VkResult vr = vkCreateRenderPass(device_, &ci, nullptr, &render_pass_);
    if (vr != VK_SUCCESS) {
        return Result<bool>::err(
            "vkCreateRenderPass failed with code " + std::to_string(static_cast<int>(vr)));
    }

    spdlog::info("Render pass created (color format: {}, depth format: D32_SFLOAT)",
                 static_cast<int>(color_format));
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Private: depth buffer
// ---------------------------------------------------------------------------

Result<bool> Renderer::create_depth_resources(VkExtent2D extent) {
    VkImageCreateInfo img_ci{};
    img_ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_ci.imageType     = VK_IMAGE_TYPE_2D;
    img_ci.format        = VK_FORMAT_D32_SFLOAT;
    img_ci.extent.width  = extent.width;
    img_ci.extent.height = extent.height;
    img_ci.extent.depth  = 1;
    img_ci.mipLevels     = 1;
    img_ci.arrayLayers   = 1;
    img_ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    img_ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    img_ci.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    img_ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_ci{};
    alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_ci.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkResult vr = vmaCreateImage(allocator_, &img_ci, &alloc_ci,
                                 &depth_image_, &depth_alloc_, nullptr);
    if (vr != VK_SUCCESS) {
        return Result<bool>::err(
            "vmaCreateImage (depth) failed with code " + std::to_string(static_cast<int>(vr)));
    }

    VkImageViewCreateInfo view_ci{};
    view_ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image                           = depth_image_;
    view_ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format                          = VK_FORMAT_D32_SFLOAT;
    view_ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_ci.subresourceRange.baseMipLevel   = 0;
    view_ci.subresourceRange.levelCount     = 1;
    view_ci.subresourceRange.baseArrayLayer = 0;
    view_ci.subresourceRange.layerCount     = 1;

    vr = vkCreateImageView(device_, &view_ci, nullptr, &depth_view_);
    if (vr != VK_SUCCESS) {
        vmaDestroyImage(allocator_, depth_image_, depth_alloc_);
        depth_image_ = VK_NULL_HANDLE;
        depth_alloc_ = VK_NULL_HANDLE;
        return Result<bool>::err(
            "vkCreateImageView (depth) failed with code " + std::to_string(static_cast<int>(vr)));
    }

    spdlog::info("Depth buffer created ({}x{}, D32_SFLOAT)", extent.width, extent.height);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Private: framebuffers
// ---------------------------------------------------------------------------

Result<bool> Renderer::create_framebuffers(const SwapchainContext& sc) {
    framebuffers_.resize(sc.image_views.size());

    for (size_t i = 0; i < sc.image_views.size(); ++i) {
        std::array<VkImageView, 2> attachments = {
            sc.image_views[i],
            depth_view_
        };

        VkFramebufferCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass      = render_pass_;
        ci.attachmentCount = static_cast<uint32_t>(attachments.size());
        ci.pAttachments    = attachments.data();
        ci.width           = sc.extent.width;
        ci.height          = sc.extent.height;
        ci.layers          = 1;

        VkResult vr = vkCreateFramebuffer(device_, &ci, nullptr, &framebuffers_[i]);
        if (vr != VK_SUCCESS) {
            // Destroy any framebuffers created so far
            for (size_t j = 0; j < i; ++j) {
                vkDestroyFramebuffer(device_, framebuffers_[j], nullptr);
            }
            framebuffers_.clear();
            return Result<bool>::err(
                "vkCreateFramebuffer failed at index " + std::to_string(i)
                + " with code " + std::to_string(static_cast<int>(vr)));
        }
    }

    spdlog::info("Created {} framebuffers", framebuffers_.size());
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Private: graphics pipeline
// ---------------------------------------------------------------------------

Result<bool> Renderer::create_pipeline() {
    // Read and compile shaders from disk
    std::string vert_source = read_file("shaders/basic.vert");
    if (vert_source.empty()) {
        return Result<bool>::err("Failed to read shaders/basic.vert");
    }

    std::string frag_source = read_file("shaders/basic.frag");
    if (frag_source.empty()) {
        return Result<bool>::err("Failed to read shaders/basic.frag");
    }

    auto vert_spirv_result = compile_glsl(vert_source, "basic.vert", shaderc_glsl_vertex_shader);
    if (vert_spirv_result.is_err()) return Result<bool>::err(vert_spirv_result.error());

    auto frag_spirv_result = compile_glsl(frag_source, "basic.frag", shaderc_glsl_fragment_shader);
    if (frag_spirv_result.is_err()) return Result<bool>::err(frag_spirv_result.error());

    auto vert_module_result = create_shader_module(device_, vert_spirv_result.value());
    if (vert_module_result.is_err()) return Result<bool>::err(vert_module_result.error());
    VkShaderModule vert_module = vert_module_result.value();

    auto frag_module_result = create_shader_module(device_, frag_spirv_result.value());
    if (frag_module_result.is_err()) {
        vkDestroyShaderModule(device_, vert_module, nullptr);
        return Result<bool>::err(frag_module_result.error());
    }
    VkShaderModule frag_module = frag_module_result.value();

    // Shader stages
    VkPipelineShaderStageCreateInfo vert_stage{};
    vert_stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_module;
    vert_stage.pName  = "main";

    VkPipelineShaderStageCreateInfo frag_stage{};
    frag_stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_module;
    frag_stage.pName  = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {vert_stage, frag_stage};

    // Vertex input: BasicVertex has position (vec3) at offset 0, normal (vec3) at offset 12
    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding   = 0;
    binding_desc.stride    = sizeof(BasicVertex);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attr_descs{};
    // location 0: position (vec3, offset 0)
    attr_descs[0].binding  = 0;
    attr_descs[0].location = 0;
    attr_descs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attr_descs[0].offset   = offsetof(BasicVertex, position);
    // location 1: normal (vec3, offset 12)
    attr_descs[1].binding  = 0;
    attr_descs[1].location = 1;
    attr_descs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attr_descs[1].offset   = offsetof(BasicVertex, normal);

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount   = 1;
    vertex_input.pVertexBindingDescriptions      = &binding_desc;
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attr_descs.size());
    vertex_input.pVertexAttributeDescriptions    = attr_descs.data();

    // Input assembly: triangles
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic state)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    // Rasterizer: solid fill, backface culling, counter-clockwise front face
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // Multisampling: disabled
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth-stencil: depth test and write enabled
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable       = VK_TRUE;
    depth_stencil.depthWriteEnable      = VK_TRUE;
    depth_stencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable     = VK_FALSE;

    // Color blending: no blending, write all channels
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                   | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable   = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments    = &blend_attachment;

    // Dynamic states: viewport and scissor
    std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates    = dynamic_states.data();

    // Pipeline layout: push constants only (no descriptor sets)
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset     = 0;
    push_range.size       = sizeof(BasicPushConstants);

    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount         = 0;
    layout_ci.pSetLayouts            = nullptr;
    layout_ci.pushConstantRangeCount = 1;
    layout_ci.pPushConstantRanges    = &push_range;

    VkResult vr = vkCreatePipelineLayout(device_, &layout_ci, nullptr, &pipeline_layout_);
    if (vr != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vert_module, nullptr);
        vkDestroyShaderModule(device_, frag_module, nullptr);
        return Result<bool>::err(
            "vkCreatePipelineLayout failed with code " + std::to_string(static_cast<int>(vr)));
    }

    // Create the graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount          = static_cast<uint32_t>(shader_stages.size());
    pipeline_ci.pStages             = shader_stages.data();
    pipeline_ci.pVertexInputState   = &vertex_input;
    pipeline_ci.pInputAssemblyState = &input_assembly;
    pipeline_ci.pViewportState      = &viewport_state;
    pipeline_ci.pRasterizationState = &rasterizer;
    pipeline_ci.pMultisampleState   = &multisampling;
    pipeline_ci.pDepthStencilState  = &depth_stencil;
    pipeline_ci.pColorBlendState    = &color_blending;
    pipeline_ci.pDynamicState       = &dynamic_state;
    pipeline_ci.layout              = pipeline_layout_;
    pipeline_ci.renderPass          = render_pass_;
    pipeline_ci.subpass             = 0;
    pipeline_ci.basePipelineHandle  = VK_NULL_HANDLE;
    pipeline_ci.basePipelineIndex   = -1;

    vr = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pipeline_);

    // Shader modules can be destroyed after pipeline creation
    vkDestroyShaderModule(device_, vert_module, nullptr);
    vkDestroyShaderModule(device_, frag_module, nullptr);

    if (vr != VK_SUCCESS) {
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
        return Result<bool>::err(
            "vkCreateGraphicsPipelines failed with code " + std::to_string(static_cast<int>(vr)));
    }

    spdlog::info("Graphics pipeline created (push constants: {} bytes)", sizeof(BasicPushConstants));
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Private: primitive mesh creation
// ---------------------------------------------------------------------------

Result<bool> Renderer::create_primitive_meshes(VkCommandPool cmd_pool, VkQueue queue) {
    // Box
    {
        auto [vertices, indices] = generate_box();
        box_mesh_ = create_mesh(vertices, indices, cmd_pool, queue);
        if (box_mesh_.vertex_buffer == VK_NULL_HANDLE) {
            return Result<bool>::err("Failed to create box mesh");
        }
        spdlog::info("Box mesh created ({} vertices, {} indices)",
                     vertices.size(), indices.size());
    }

    // Sphere
    {
        auto [vertices, indices] = generate_sphere(16);
        sphere_mesh_ = create_mesh(vertices, indices, cmd_pool, queue);
        if (sphere_mesh_.vertex_buffer == VK_NULL_HANDLE) {
            return Result<bool>::err("Failed to create sphere mesh");
        }
        spdlog::info("Sphere mesh created ({} vertices, {} indices)",
                     vertices.size(), indices.size());
    }

    // Ground plane
    {
        auto [vertices, indices] = generate_ground_plane(100.0f);
        ground_mesh_ = create_mesh(vertices, indices, cmd_pool, queue);
        if (ground_mesh_.vertex_buffer == VK_NULL_HANDLE) {
            return Result<bool>::err("Failed to create ground plane mesh");
        }
        spdlog::info("Ground plane mesh created ({} vertices, {} indices)",
                     vertices.size(), indices.size());
    }

    // Cylinder
    {
        auto [vertices, indices] = generate_cylinder(16);
        cylinder_mesh_ = create_mesh(vertices, indices, cmd_pool, queue);
        if (cylinder_mesh_.vertex_buffer == VK_NULL_HANDLE) {
            return Result<bool>::err("Failed to create cylinder mesh");
        }
        spdlog::info("Cylinder mesh created ({} vertices, {} indices)",
                     vertices.size(), indices.size());
    }

    return Result<bool>::ok(true);
}

PrimitiveMesh Renderer::create_mesh(
    const std::vector<BasicVertex>& vertices,
    const std::vector<uint32_t>& indices,
    VkCommandPool cmd_pool,
    VkQueue queue)
{
    PrimitiveMesh mesh;
    mesh.index_count = static_cast<uint32_t>(indices.size());

    VkDeviceSize vertex_size = vertices.size() * sizeof(BasicVertex);
    VkDeviceSize index_size  = indices.size() * sizeof(uint32_t);

    // --- Create device-local vertex buffer ---
    {
        VkBufferCreateInfo buf_ci{};
        buf_ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_ci.size        = vertex_size;
        buf_ci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_ci{};
        alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;

        VkResult vr = vmaCreateBuffer(allocator_, &buf_ci, &alloc_ci,
                                      &mesh.vertex_buffer, &mesh.vertex_alloc, nullptr);
        if (vr != VK_SUCCESS) {
            spdlog::error("Failed to create vertex buffer: {}", static_cast<int>(vr));
            return {};
        }
    }

    // --- Create device-local index buffer ---
    {
        VkBufferCreateInfo buf_ci{};
        buf_ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_ci.size        = index_size;
        buf_ci.usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_ci{};
        alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;

        VkResult vr = vmaCreateBuffer(allocator_, &buf_ci, &alloc_ci,
                                      &mesh.index_buffer, &mesh.index_alloc, nullptr);
        if (vr != VK_SUCCESS) {
            spdlog::error("Failed to create index buffer: {}", static_cast<int>(vr));
            vmaDestroyBuffer(allocator_, mesh.vertex_buffer, mesh.vertex_alloc);
            return {};
        }
    }

    // --- Create staging buffer and upload vertex + index data ---
    VkDeviceSize staging_size = vertex_size + index_size;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_alloc = VK_NULL_HANDLE;

    {
        VkBufferCreateInfo buf_ci{};
        buf_ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_ci.size        = staging_size;
        buf_ci.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_ci{};
        alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                       | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo alloc_info{};
        VkResult vr = vmaCreateBuffer(allocator_, &buf_ci, &alloc_ci,
                                      &staging_buffer, &staging_alloc, &alloc_info);
        if (vr != VK_SUCCESS) {
            spdlog::error("Failed to create staging buffer: {}", static_cast<int>(vr));
            vmaDestroyBuffer(allocator_, mesh.vertex_buffer, mesh.vertex_alloc);
            vmaDestroyBuffer(allocator_, mesh.index_buffer, mesh.index_alloc);
            return {};
        }

        // Copy vertex data, then index data, into the staging buffer
        auto* mapped = static_cast<uint8_t*>(alloc_info.pMappedData);
        std::memcpy(mapped, vertices.data(), vertex_size);
        std::memcpy(mapped + vertex_size, indices.data(), index_size);
    }

    // Record copy commands
    VkCommandBuffer cmd = begin_single_time_commands(device_, cmd_pool);

    VkBufferCopy vertex_copy{};
    vertex_copy.srcOffset = 0;
    vertex_copy.dstOffset = 0;
    vertex_copy.size      = vertex_size;
    vkCmdCopyBuffer(cmd, staging_buffer, mesh.vertex_buffer, 1, &vertex_copy);

    VkBufferCopy index_copy{};
    index_copy.srcOffset = vertex_size;
    index_copy.dstOffset = 0;
    index_copy.size      = index_size;
    vkCmdCopyBuffer(cmd, staging_buffer, mesh.index_buffer, 1, &index_copy);

    end_single_time_commands(device_, queue, cmd_pool, cmd);

    // Clean up staging buffer
    vmaDestroyBuffer(allocator_, staging_buffer, staging_alloc);

    return mesh;
}

void Renderer::destroy_mesh(PrimitiveMesh& mesh) {
    if (mesh.vertex_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, mesh.vertex_buffer, mesh.vertex_alloc);
        mesh.vertex_buffer = VK_NULL_HANDLE;
        mesh.vertex_alloc  = VK_NULL_HANDLE;
    }
    if (mesh.index_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, mesh.index_buffer, mesh.index_alloc);
        mesh.index_buffer = VK_NULL_HANDLE;
        mesh.index_alloc  = VK_NULL_HANDLE;
    }
    mesh.index_count = 0;
}

// ---------------------------------------------------------------------------
// Static: mesh generators
// ---------------------------------------------------------------------------

std::pair<std::vector<BasicVertex>, std::vector<uint32_t>> Renderer::generate_box() {
    // Unit cube centered at origin (-0.5 to 0.5 on each axis).
    // Each face has 4 vertices with the face normal, 6 indices (2 triangles).

    std::vector<BasicVertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(24);
    indices.reserve(36);

    // Helper: add a face (4 vertices, 6 indices)
    auto add_face = [&](vec3 p0, vec3 p1, vec3 p2, vec3 p3, vec3 normal) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({p0, normal});
        vertices.push_back({p1, normal});
        vertices.push_back({p2, normal});
        vertices.push_back({p3, normal});
        // Two triangles: 0-1-2, 0-2-3
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    };

    const float h = 0.5f;

    // +Z face (front)
    add_face({-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}, { 0,  0,  1});
    // -Z face (back)
    add_face({ h, -h, -h}, {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h}, { 0,  0, -1});
    // +X face (right)
    add_face({ h, -h,  h}, { h, -h, -h}, { h,  h, -h}, { h,  h,  h}, { 1,  0,  0});
    // -X face (left)
    add_face({-h, -h, -h}, {-h, -h,  h}, {-h,  h,  h}, {-h,  h, -h}, {-1,  0,  0});
    // +Y face (top)
    add_face({-h,  h,  h}, { h,  h,  h}, { h,  h, -h}, {-h,  h, -h}, { 0,  1,  0});
    // -Y face (bottom)
    add_face({-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h}, { 0, -1,  0});

    return {vertices, indices};
}

std::pair<std::vector<BasicVertex>, std::vector<uint32_t>> Renderer::generate_sphere(int segments) {
    // UV sphere with `segments` longitude slices and `segments/2` latitude bands.
    // Radius = 0.5 (unit diameter).

    const int rings = segments / 2;
    const float radius = 0.5f;
    const float pi = 3.14159265358979323846f;

    std::vector<BasicVertex> vertices;
    std::vector<uint32_t> indices;

    // Generate vertices
    for (int ring = 0; ring <= rings; ++ring) {
        float phi = pi * static_cast<float>(ring) / static_cast<float>(rings);
        float sin_phi = std::sin(phi);
        float cos_phi = std::cos(phi);

        for (int seg = 0; seg <= segments; ++seg) {
            float theta = 2.0f * pi * static_cast<float>(seg) / static_cast<float>(segments);
            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);

            vec3 normal{sin_phi * cos_theta, cos_phi, sin_phi * sin_theta};
            vec3 position = normal * radius;

            vertices.push_back({position, normal});
        }
    }

    // Generate indices
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            uint32_t current     = static_cast<uint32_t>(ring * (segments + 1) + seg);
            uint32_t next        = current + 1;
            uint32_t below       = current + static_cast<uint32_t>(segments + 1);
            uint32_t below_next  = below + 1;

            // Two triangles per quad
            indices.push_back(current);
            indices.push_back(below);
            indices.push_back(next);

            indices.push_back(next);
            indices.push_back(below);
            indices.push_back(below_next);
        }
    }

    return {vertices, indices};
}

std::pair<std::vector<BasicVertex>, std::vector<uint32_t>> Renderer::generate_ground_plane(float size) {
    // Large flat quad on the XZ plane at Y = 0.
    // Normal faces up (+Y).

    float h = size * 0.5f;
    vec3 normal{0.0f, 1.0f, 0.0f};

    std::vector<BasicVertex> vertices = {
        {{-h, 0.0f,  h}, normal},  // 0: front-left
        {{ h, 0.0f,  h}, normal},  // 1: front-right
        {{ h, 0.0f, -h}, normal},  // 2: back-right
        {{-h, 0.0f, -h}, normal},  // 3: back-left
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,
        0, 2, 3
    };

    return {vertices, indices};
}

std::pair<std::vector<BasicVertex>, std::vector<uint32_t>> Renderer::generate_cylinder(int segments) {
    // Unit cylinder: radius 0.5, height 1.0 (-0.5 to +0.5 on Y), centered at origin.
    const float radius = 0.5f;
    const float half_h = 0.5f;
    const float pi = 3.14159265358979323846f;

    std::vector<BasicVertex> vertices;
    std::vector<uint32_t> indices;

    // --- Side vertices (two rings) ---
    for (int i = 0; i <= segments; ++i) {
        float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        float ct = std::cos(theta);
        float st = std::sin(theta);
        vec3 normal{ct, 0.0f, st};

        vertices.push_back({{ct * radius, -half_h, st * radius}, normal}); // bottom ring
        vertices.push_back({{ct * radius,  half_h, st * radius}, normal}); // top ring
    }

    // Side indices
    for (int i = 0; i < segments; ++i) {
        uint32_t bl = static_cast<uint32_t>(i * 2);
        uint32_t tl = bl + 1;
        uint32_t br = bl + 2;
        uint32_t tr = bl + 3;
        indices.push_back(bl); indices.push_back(br); indices.push_back(tl);
        indices.push_back(tl); indices.push_back(br); indices.push_back(tr);
    }

    // --- Top cap ---
    uint32_t top_center = static_cast<uint32_t>(vertices.size());
    vertices.push_back({{0.0f, half_h, 0.0f}, {0.0f, 1.0f, 0.0f}});
    uint32_t top_ring_start = static_cast<uint32_t>(vertices.size());
    for (int i = 0; i < segments; ++i) {
        float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        vertices.push_back({{std::cos(theta) * radius, half_h, std::sin(theta) * radius},
                            {0.0f, 1.0f, 0.0f}});
    }
    for (int i = 0; i < segments; ++i) {
        uint32_t next = (i + 1) % segments;
        indices.push_back(top_center);
        indices.push_back(top_ring_start + static_cast<uint32_t>(i));
        indices.push_back(top_ring_start + static_cast<uint32_t>(next));
    }

    // --- Bottom cap ---
    uint32_t bot_center = static_cast<uint32_t>(vertices.size());
    vertices.push_back({{0.0f, -half_h, 0.0f}, {0.0f, -1.0f, 0.0f}});
    uint32_t bot_ring_start = static_cast<uint32_t>(vertices.size());
    for (int i = 0; i < segments; ++i) {
        float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        vertices.push_back({{std::cos(theta) * radius, -half_h, std::sin(theta) * radius},
                            {0.0f, -1.0f, 0.0f}});
    }
    for (int i = 0; i < segments; ++i) {
        uint32_t next = (i + 1) % segments;
        indices.push_back(bot_center);
        indices.push_back(bot_ring_start + static_cast<uint32_t>(next));
        indices.push_back(bot_ring_start + static_cast<uint32_t>(i));
    }

    return {vertices, indices};
}

} // namespace odyssey::vulkan
