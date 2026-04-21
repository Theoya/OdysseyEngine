#include "editor/scene_viewport_renderer.h"

#include <shaderc/shaderc.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <cstring>

namespace odyssey::editor {

// ---------------------------------------------------------------------------
// Embedded GLSL — kept tiny and self-documenting. Compiled at init time via
// shaderc (already a vcpkg dep — no new dependency).
// ---------------------------------------------------------------------------

static const char* kVertGlsl = R"GLSL(
#version 450
layout(location = 0) in vec3 in_pos;
layout(location = 0) out vec3 v_local;

// Push constants: 64B MVP, 16B color, 16B scale+pad.
layout(push_constant) uniform PC {
    mat4 mvp;
    vec4 color;
    vec4 scale;  // xyz = scale, w unused
} pc;

void main() {
    vec3 scaled = in_pos * pc.scale.xyz;
    gl_Position = pc.mvp * vec4(scaled, 1.0);
    v_local = in_pos;
}
)GLSL";

static const char* kFragGlsl = R"GLSL(
#version 450
layout(location = 0) in vec3 v_local;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PC {
    mat4 mvp;
    vec4 color;
    vec4 scale;
} pc;

void main() {
    // Face-normal shading via derivatives — gives cheap faceted shading
    // that preserves the impressionist feel without needing vertex normals.
    vec3 dx = dFdx(v_local);
    vec3 dy = dFdy(v_local);
    vec3 n  = normalize(cross(dx, dy));
    float lambert = max(0.25, dot(n, normalize(vec3(0.4, 1.0, 0.3))));
    out_color = vec4(pc.color.rgb * lambert, pc.color.a);
}
)GLSL";

// Push constant layout — keep aligned to the GLSL.
struct PushConstants {
    float mvp[16];
    float color[4];
    float scale[4];
};

// ---------------------------------------------------------------------------
// Ctor / dtor
// ---------------------------------------------------------------------------

SceneViewportRenderer::~SceneViewportRenderer() {
    shutdown();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

uint32_t SceneViewportRenderer::find_memory_type(
    uint32_t type_filter, VkMemoryPropertyFlags props) const
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(phys_device_, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

static VkShaderModule compile_glsl(VkDevice device,
                                   const char* src,
                                   shaderc_shader_kind kind,
                                   const char* name) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetEnvironment(shaderc_target_env_vulkan,
                                 shaderc_env_version_vulkan_1_3);
    auto result = compiler.CompileGlslToSpv(src, kind, name, options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        spdlog::error("[editor viewport] shader {} failed: {}", name,
                      result.GetErrorMessage());
        return VK_NULL_HANDLE;
    }
    std::vector<uint32_t> spirv(result.cbegin(), result.cend());

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spirv.size() * sizeof(uint32_t);
    ci.pCode    = spirv.data();
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

// ---------------------------------------------------------------------------
// Image creation
// ---------------------------------------------------------------------------

Result<bool> SceneViewportRenderer::create_images(VkExtent2D extent) {
    extent_ = extent;

    // ---- Color ----
    {
        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = color_format_;
        ci.extent      = {extent.width, extent.height, 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device_, &ci, nullptr, &color_image_) != VK_SUCCESS) {
            return Result<bool>::err("vkCreateImage(color) failed");
        }
        VkMemoryRequirements mr{};
        vkGetImageMemoryRequirements(device_, color_image_, &mr);
        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = find_memory_type(
            mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX) {
            return Result<bool>::err("no device-local heap for color image");
        }
        if (vkAllocateMemory(device_, &ai, nullptr, &color_memory_) != VK_SUCCESS) {
            return Result<bool>::err("vkAllocateMemory(color) failed");
        }
        vkBindImageMemory(device_, color_image_, color_memory_, 0);

        VkImageViewCreateInfo vci{};
        vci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image    = color_image_;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format   = color_format_;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &vci, nullptr, &offscreen_view_) != VK_SUCCESS) {
            return Result<bool>::err("vkCreateImageView(color) failed");
        }

        VkSamplerCreateInfo sci{};
        sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter    = VK_FILTER_LINEAR;
        sci.minFilter    = VK_FILTER_LINEAR;
        sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.maxLod       = 1.0f;
        if (vkCreateSampler(device_, &sci, nullptr, &offscreen_sampler_) != VK_SUCCESS) {
            return Result<bool>::err("vkCreateSampler(offscreen) failed");
        }
    }

    // ---- Depth ----
    {
        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = VK_FORMAT_D32_SFLOAT;
        ci.extent      = {extent.width, extent.height, 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device_, &ci, nullptr, &depth_image_) != VK_SUCCESS) {
            return Result<bool>::err("vkCreateImage(depth) failed");
        }
        VkMemoryRequirements mr{};
        vkGetImageMemoryRequirements(device_, depth_image_, &mr);
        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = find_memory_type(
            mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX) {
            return Result<bool>::err("no device-local heap for depth image");
        }
        if (vkAllocateMemory(device_, &ai, nullptr, &depth_memory_) != VK_SUCCESS) {
            return Result<bool>::err("vkAllocateMemory(depth) failed");
        }
        vkBindImageMemory(device_, depth_image_, depth_memory_, 0);

        VkImageViewCreateInfo vci{};
        vci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image    = depth_image_;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format   = VK_FORMAT_D32_SFLOAT;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &vci, nullptr, &depth_view_) != VK_SUCCESS) {
            return Result<bool>::err("vkCreateImageView(depth) failed");
        }
    }

    return Result<bool>::ok(true);
}

void SceneViewportRenderer::destroy_images() {
    if (framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, framebuffer_, nullptr);
        framebuffer_ = VK_NULL_HANDLE;
    }
    if (offscreen_sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, offscreen_sampler_, nullptr);
        offscreen_sampler_ = VK_NULL_HANDLE;
    }
    if (offscreen_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, offscreen_view_, nullptr);
        offscreen_view_ = VK_NULL_HANDLE;
    }
    if (color_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, color_image_, nullptr);
        color_image_ = VK_NULL_HANDLE;
    }
    if (color_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, color_memory_, nullptr);
        color_memory_ = VK_NULL_HANDLE;
    }
    if (depth_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depth_view_, nullptr);
        depth_view_ = VK_NULL_HANDLE;
    }
    if (depth_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depth_image_, nullptr);
        depth_image_ = VK_NULL_HANDLE;
    }
    if (depth_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depth_memory_, nullptr);
        depth_memory_ = VK_NULL_HANDLE;
    }
}

// ---------------------------------------------------------------------------
// Render pass & framebuffer
// ---------------------------------------------------------------------------

Result<bool> SceneViewportRenderer::create_render_pass() {
    VkAttachmentDescription color{};
    color.format         = color_format_;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // for ImGui sample

    VkAttachmentDescription depth{};
    depth.format         = VK_FORMAT_D32_SFLOAT;
    depth.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentDescription, 2> atts{color, depth};

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 1;
    depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount    = 1;
    sub.pColorAttachments       = &color_ref;
    sub.pDepthStencilAttachment = &depth_ref;

    // Explicit color→sample dependency so ImGui's fragment-shader read of
    // offscreen_view_ is ordered after the render pass writes. The subpass
    // end's implicit transition (COLOR_ATTACHMENT_OPTIMAL →
    // SHADER_READ_ONLY_OPTIMAL) is made visible via this dependency.
    VkSubpassDependency dep{};
    dep.srcSubpass    = 0;
    dep.dstSubpass    = VK_SUBPASS_EXTERNAL;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = static_cast<uint32_t>(atts.size());
    ci.pAttachments    = atts.data();
    ci.subpassCount    = 1;
    ci.pSubpasses      = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;

    if (vkCreateRenderPass(device_, &ci, nullptr, &render_pass_) != VK_SUCCESS) {
        return Result<bool>::err("vkCreateRenderPass(editor viewport) failed");
    }
    return Result<bool>::ok(true);
}

Result<bool> SceneViewportRenderer::create_framebuffer() {
    std::array<VkImageView, 2> attachments{offscreen_view_, depth_view_};
    VkFramebufferCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass      = render_pass_;
    ci.attachmentCount = static_cast<uint32_t>(attachments.size());
    ci.pAttachments    = attachments.data();
    ci.width           = extent_.width;
    ci.height          = extent_.height;
    ci.layers          = 1;
    if (vkCreateFramebuffer(device_, &ci, nullptr, &framebuffer_) != VK_SUCCESS) {
        return Result<bool>::err("vkCreateFramebuffer(editor viewport) failed");
    }
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Pipeline
// ---------------------------------------------------------------------------

Result<bool> SceneViewportRenderer::create_pipeline() {
    vert_module_ = compile_glsl(device_, kVertGlsl, shaderc_vertex_shader, "viewport.vert");
    if (vert_module_ == VK_NULL_HANDLE) {
        return Result<bool>::err("viewport vertex shader compile failed");
    }
    frag_module_ = compile_glsl(device_, kFragGlsl, shaderc_fragment_shader, "viewport.frag");
    if (frag_module_ == VK_NULL_HANDLE) {
        return Result<bool>::err("viewport fragment shader compile failed");
    }

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset     = 0;
    pc_range.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.pushConstantRangeCount = 1;
    layout_ci.pPushConstantRanges    = &pc_range;
    if (vkCreatePipelineLayout(device_, &layout_ci, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        return Result<bool>::err("vkCreatePipelineLayout(viewport) failed");
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_module_;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_module_;
    stages[1].pName  = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(float) * 3;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attr{};
    attr.binding  = 0;
    attr.location = 0;
    attr.format   = VK_FORMAT_R32G32B32_SFLOAT;
    attr.offset   = 0;

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = 1;
    vi.pVertexBindingDescriptions      = &binding;
    vi.vertexAttributeDescriptionCount = 1;
    vi.pVertexAttributeDescriptions    = &attr;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_BACK_BIT;
    rs.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable    = VK_FALSE;
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    std::array<VkDynamicState, 2> dyn_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = static_cast<uint32_t>(dyn_states.size());
    dyn.pDynamicStates    = dyn_states.data();

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount          = static_cast<uint32_t>(stages.size());
    pci.pStages             = stages.data();
    pci.pVertexInputState   = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &cb;
    pci.pDynamicState       = &dyn;
    pci.layout              = pipeline_layout_;
    pci.renderPass          = render_pass_;
    pci.subpass             = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_)
        != VK_SUCCESS) {
        return Result<bool>::err("vkCreateGraphicsPipelines(viewport) failed");
    }
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Cube mesh: 24 unique corners (to get flat shading via derivatives),
// 12 triangles (36 indices). Unit cube centered at origin (-0.5..+0.5).
// ---------------------------------------------------------------------------

Result<bool> SceneViewportRenderer::create_cube_mesh() {
    // 8 cube corners shared; 24 indices = 12 tris, indexed.
    const float vtx[] = {
        // 8 corners of a unit cube centered on origin.
        -0.5f, -0.5f, -0.5f,  // 0
         0.5f, -0.5f, -0.5f,  // 1
         0.5f,  0.5f, -0.5f,  // 2
        -0.5f,  0.5f, -0.5f,  // 3
        -0.5f, -0.5f,  0.5f,  // 4
         0.5f, -0.5f,  0.5f,  // 5
         0.5f,  0.5f,  0.5f,  // 6
        -0.5f,  0.5f,  0.5f,  // 7
    };
    // 12 triangles, CW winding (matches rasterizer FRONT_FACE_CLOCKWISE).
    const uint16_t idx[] = {
        // -Z face (back)
        0, 2, 1,  0, 3, 2,
        // +Z face (front)
        4, 5, 6,  4, 6, 7,
        // -X face
        0, 4, 7,  0, 7, 3,
        // +X face
        1, 2, 6,  1, 6, 5,
        // -Y face
        0, 1, 5,  0, 5, 4,
        // +Y face
        3, 7, 6,  3, 6, 2,
    };
    index_count_ = sizeof(idx) / sizeof(idx[0]);

    // Helper: create a host-visible+coherent buffer and memcpy data.
    auto make_buffer = [&](VkBufferUsageFlags usage, const void* data, size_t size,
                           VkBuffer& out_buf, VkDeviceMemory& out_mem) -> Result<bool> {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = size;
        bi.usage       = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bi, nullptr, &out_buf) != VK_SUCCESS) {
            return Result<bool>::err("vkCreateBuffer failed");
        }
        VkMemoryRequirements mr{};
        vkGetBufferMemoryRequirements(device_, out_buf, &mr);
        VkMemoryAllocateInfo ai{};
        ai.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = find_memory_type(
            mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (ai.memoryTypeIndex == UINT32_MAX) {
            return Result<bool>::err("no host-visible heap");
        }
        if (vkAllocateMemory(device_, &ai, nullptr, &out_mem) != VK_SUCCESS) {
            return Result<bool>::err("vkAllocateMemory(buffer) failed");
        }
        vkBindBufferMemory(device_, out_buf, out_mem, 0);
        void* mapped = nullptr;
        vkMapMemory(device_, out_mem, 0, size, 0, &mapped);
        std::memcpy(mapped, data, size);
        vkUnmapMemory(device_, out_mem);
        return Result<bool>::ok(true);
    };

    auto r = make_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vtx, sizeof(vtx), vertex_buffer_, vertex_memory_);
    if (r.is_err()) return r;
    r = make_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    idx, sizeof(idx), index_buffer_, index_memory_);
    if (r.is_err()) return r;
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

Result<bool> SceneViewportRenderer::initialize(const SceneViewportInit& init) {
    phys_device_     = init.phys_device;
    device_          = init.device;
    graphics_queue_  = init.graphics_queue;
    graphics_family_ = init.graphics_family;
    color_format_    = init.color_format;

    auto r = create_images(init.initial_extent);
    if (r.is_err()) return r;
    r = create_render_pass();
    if (r.is_err()) return r;
    r = create_framebuffer();
    if (r.is_err()) return r;
    r = create_pipeline();
    if (r.is_err()) return r;
    r = create_cube_mesh();
    if (r.is_err()) return r;

    spdlog::info("[editor viewport] initialized {}x{}",
                 extent_.width, extent_.height);
    return Result<bool>::ok(true);
}

void SceneViewportRenderer::shutdown() {
    if (device_ == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device_);

    destroy_images();

    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
    if (vert_module_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, vert_module_, nullptr);
        vert_module_ = VK_NULL_HANDLE;
    }
    if (frag_module_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, frag_module_, nullptr);
        frag_module_ = VK_NULL_HANDLE;
    }
    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }
    if (vertex_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, vertex_buffer_, nullptr);
        vertex_buffer_ = VK_NULL_HANDLE;
    }
    if (vertex_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, vertex_memory_, nullptr);
        vertex_memory_ = VK_NULL_HANDLE;
    }
    if (index_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, index_buffer_, nullptr);
        index_buffer_ = VK_NULL_HANDLE;
    }
    if (index_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, index_memory_, nullptr);
        index_memory_ = VK_NULL_HANDLE;
    }
    device_ = VK_NULL_HANDLE;
}

Result<bool> SceneViewportRenderer::resize(VkExtent2D new_extent) {
    if (new_extent.width == 0 || new_extent.height == 0) {
        return Result<bool>::err("resize to zero extent");
    }
    if (new_extent.width == extent_.width &&
        new_extent.height == extent_.height) {
        return Result<bool>::ok(true);
    }
    vkDeviceWaitIdle(device_);
    destroy_images();
    auto r = create_images(new_extent);
    if (r.is_err()) return r;
    r = create_framebuffer();
    if (r.is_err()) return r;
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Record
// ---------------------------------------------------------------------------

void SceneViewportRenderer::record(
    VkCommandBuffer cmd,
    const float vp[16],
    const std::vector<SceneDrawEntity>& entities)
{
    if (render_pass_ == VK_NULL_HANDLE || framebuffer_ == VK_NULL_HANDLE) return;

    std::array<VkClearValue, 2> clears{};
    // Liminal mood clear — dusk violet, matches lighting profile.
    clears[0].color        = {{0.06f, 0.06f, 0.10f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{};
    rp.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass        = render_pass_;
    rp.framebuffer       = framebuffer_;
    rp.renderArea.extent = extent_;
    rp.clearValueCount   = static_cast<uint32_t>(clears.size());
    rp.pClearValues      = clears.data();

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vkvp{};
    vkvp.width    = static_cast<float>(extent_.width);
    vkvp.height   = static_cast<float>(extent_.height);
    vkvp.minDepth = 0.0f;
    vkvp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vkvp);
    VkRect2D sc{};
    sc.extent = extent_;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    VkDeviceSize vo = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer_, &vo);
    vkCmdBindIndexBuffer(cmd, index_buffer_, 0, VK_INDEX_TYPE_UINT16);

    for (const auto& e : entities) {
        PushConstants pc{};
        // MVP = VP * translate(position). Pure column-major multiply with
        // VP treated column-major (glm default). Formula:
        //   result[col][row] = sum_k VP[k][row] * T[col][k]
        //   T = [[1,0,0,0],[0,1,0,0],[0,0,1,0],[px,py,pz,1]]
        //   → only the last column of the result changes from VP:
        //     last_col = VP * [px, py, pz, 1]^T
        //     other columns = VP columns
        // Derivation: model = translate(p), so VP*model is VP with an added
        // translation applied to each transformed point. The translation
        // shows up only in the homogeneous output of column 3.
        for (int i = 0; i < 16; ++i) pc.mvp[i] = vp[i];
        // Rewrite columns 0..2 (they stay as VP's columns) and column 3
        // becomes VP * [px, py, pz, 1].
        const float px = e.position[0];
        const float py = e.position[1];
        const float pz = e.position[2];
        // Column-major vp layout: vp[col*4+row]. We want:
        //   new_col3[row] = vp[0*4+row]*px + vp[1*4+row]*py + vp[2*4+row]*pz + vp[3*4+row]
        for (int row = 0; row < 4; ++row) {
            pc.mvp[3 * 4 + row] = vp[0 * 4 + row] * px
                                + vp[1 * 4 + row] * py
                                + vp[2 * 4 + row] * pz
                                + vp[3 * 4 + row];
        }
        pc.color[0] = e.color[0];
        pc.color[1] = e.color[1];
        pc.color[2] = e.color[2];
        pc.color[3] = e.color[3];
        pc.scale[0] = e.scale[0];
        pc.scale[1] = e.scale[1];
        pc.scale[2] = e.scale[2];
        pc.scale[3] = 0.0f;

        vkCmdPushConstants(cmd, pipeline_layout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pc);
        vkCmdDrawIndexed(cmd, index_count_, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
}

} // namespace odyssey::editor
