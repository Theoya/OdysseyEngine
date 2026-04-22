#include "vulkan/bindless_texture_registry.h"
#include "vulkan/command.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cassert>
#include <cstring>

namespace odyssey::vulkan {

std::string registry_err_to_string(RegistryErr e) {
    switch (e) {
        case RegistryErr::AllocatorFull:           return "RegistryErr::AllocatorFull";
        case RegistryErr::VulkanImageCreateFailed: return "RegistryErr::VulkanImageCreateFailed";
        case RegistryErr::VulkanImageViewFailed:   return "RegistryErr::VulkanImageViewFailed";
        case RegistryErr::VulkanSamplerFailed:     return "RegistryErr::VulkanSamplerFailed";
        case RegistryErr::VulkanDescriptorFailed:  return "RegistryErr::VulkanDescriptorFailed";
        case RegistryErr::StagingFailed:           return "RegistryErr::StagingFailed";
        case RegistryErr::InvalidHandle:           return "RegistryErr::InvalidHandle";
    }
    return "RegistryErr::Unknown";
}

// ---------------------------------------------------------------------------
// Destructor / shutdown
// ---------------------------------------------------------------------------

BindlessTextureRegistry::~BindlessTextureRegistry() {
    shutdown();
}

void BindlessTextureRegistry::shutdown() {
    if (device_ == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device_);

    // Destroy all resident textures.
    for (auto& entry : entries_) {
        destroy_texture_entry(entry);
    }
    entries_.clear();
    path_map_.clear();

    // Samplers.
    for (auto& s : samplers_) {
        if (s != VK_NULL_HANDLE) {
            vkDestroySampler(device_, s, nullptr);
            s = VK_NULL_HANDLE;
        }
    }

    // Descriptor set freed implicitly with the pool.
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }
    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
        descriptor_set_layout_ = VK_NULL_HANDLE;
    }

    spdlog::info("BindlessTextureRegistry destroyed");
    device_ = VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

Result<bool> BindlessTextureRegistry::initialize(const DeviceContext& device_ctx,
                                                   VkCommandPool command_pool) {
    device_    = device_ctx.device;
    vma_       = device_ctx.allocator;
    queue_     = device_ctx.graphics_queue;

    entries_.resize(MAX_BINDLESS_TEXTURES);

    auto r = create_descriptor_layout();
    if (r.is_err()) return r;

    r = create_descriptor_pool();
    if (r.is_err()) return r;

    // Allocate the one bindless descriptor set.
    {
        uint32_t var_count = MAX_BINDLESS_TEXTURES;
        VkDescriptorSetVariableDescriptorCountAllocateInfo var_ci{};
        var_ci.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        var_ci.descriptorSetCount = 1;
        var_ci.pDescriptorCounts  = &var_count;

        VkDescriptorSetAllocateInfo alloc_ci{};
        alloc_ci.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_ci.pNext              = &var_ci;
        alloc_ci.descriptorPool     = descriptor_pool_;
        alloc_ci.descriptorSetCount = 1;
        alloc_ci.pSetLayouts        = &descriptor_set_layout_;

        VkResult vr = vkAllocateDescriptorSets(device_, &alloc_ci, &descriptor_set_);
        if (vr != VK_SUCCESS) {
            return Result<bool>::err("vkAllocateDescriptorSets (bindless) failed: "
                                     + std::to_string(static_cast<int>(vr)));
        }
    }

    r = create_samplers();
    if (r.is_err()) return r;

    r = upload_sentinel(command_pool);
    if (r.is_err()) return r;

    spdlog::info("BindlessTextureRegistry initialized ({} slots, sentinel at slot 0)",
                 MAX_BINDLESS_TEXTURES);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------

Result<TextureHandle> BindlessTextureRegistry::load(const std::filesystem::path& abs_path,
                                                      const uint8_t* rgba_pixels,
                                                      uint32_t width, uint32_t height,
                                                      VkCommandPool command_pool) {
    // Dedup by resolved absolute path (3d-modeler condition).
    std::string key = abs_path.string();
    auto it = path_map_.find(key);
    if (it != path_map_.end()) {
        return Result<TextureHandle>::ok(it->second);
    }

    // Allocate a slot.
    auto alloc_result = slot_alloc_.alloc();
    if (alloc_result.is_err()) {
        return Result<TextureHandle>::err(
            "BindlessTextureRegistry: slot table full — " + alloc_err_to_string(alloc_result.error()));
    }
    TextureHandle handle = alloc_result.value();
    uint32_t slot = handle.slot();

    auto upload_result = upload_texture(rgba_pixels, width, height, command_pool, slot);
    if (upload_result.is_err()) {
        // Roll back the slot allocation.
        slot_alloc_.free(handle);
        return upload_result;
    }

    // Store in dedup map.
    path_map_[key] = handle;

    spdlog::debug("BindlessTextureRegistry: loaded '{}' → slot {} gen {}",
                  key, slot, handle.generation());
    return Result<TextureHandle>::ok(handle);
}

// ---------------------------------------------------------------------------
// unload
// ---------------------------------------------------------------------------

Result<bool> BindlessTextureRegistry::unload(TextureHandle handle) {
    if (!handle.is_valid() || handle.is_sentinel()) {
        return Result<bool>::err("BindlessTextureRegistry::unload: invalid or sentinel handle");
    }

    uint32_t slot = handle.slot();
    if (slot >= MAX_BINDLESS_TEXTURES || !slot_alloc_.is_occupied(slot)) {
        return Result<bool>::err("BindlessTextureRegistry::unload: handle not occupied");
    }

    // Verify generation.
    if (slot_alloc_.generation_of(slot) != handle.generation()) {
        return Result<bool>::err("BindlessTextureRegistry::unload: stale handle (generation mismatch)");
    }

    // Write magenta sentinel descriptor to freed slot BEFORE returning it to
    // the free list (asset-engineer condition: sentinel written before freelist return).
    write_descriptor(slot, entries_[0].view, samplers_[0]);

    // Destroy GPU resources for this slot.
    destroy_texture_entry(entries_[slot]);

    // Remove from path map.
    for (auto it = path_map_.begin(); it != path_map_.end(); ) {
        if (it->second == handle) {
            it = path_map_.erase(it);
        } else {
            ++it;
        }
    }

    // Return slot to free list (generation bumped inside slot_alloc_.free()).
    auto fr = slot_alloc_.free(handle);
    if (fr.is_err()) {
        return Result<bool>::err("BindlessTextureRegistry::unload: free() failed: "
                                 + alloc_err_to_string(fr.error()));
    }

    spdlog::debug("BindlessTextureRegistry: unloaded slot {}", slot);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// find
// ---------------------------------------------------------------------------

TextureHandle BindlessTextureRegistry::find(const std::filesystem::path& abs_path) const {
    auto it = path_map_.find(abs_path.string());
    if (it == path_map_.end()) return TextureHandle{};
    return it->second;
}

// ---------------------------------------------------------------------------
// Private: create descriptor set layout
// ---------------------------------------------------------------------------

Result<bool> BindlessTextureRegistry::create_descriptor_layout() {
    // Binding 0: combined-image-sampler array of MAX_BINDLESS_TEXTURES.
    // Flags: UPDATE_AFTER_BIND, PARTIALLY_BOUND, VARIABLE_DESCRIPTOR_COUNT.
    // (See M4 artifact in device.h for flag derivations.)
    VkDescriptorSetLayoutBinding binding{};
    binding.binding         = 0;
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = MAX_BINDLESS_TEXTURES;
    binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorBindingFlags binding_flags =
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT      |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT         |
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
    flags_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flags_ci.bindingCount  = 1;
    flags_ci.pBindingFlags = &binding_flags;

    VkDescriptorSetLayoutCreateInfo layout_ci{};
    layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.pNext        = &flags_ci;
    layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layout_ci.bindingCount = 1;
    layout_ci.pBindings    = &binding;

    VkResult vr = vkCreateDescriptorSetLayout(device_, &layout_ci, nullptr, &descriptor_set_layout_);
    if (vr != VK_SUCCESS) {
        return Result<bool>::err("vkCreateDescriptorSetLayout (bindless) failed: "
                                 + std::to_string(static_cast<int>(vr)));
    }

    spdlog::info("Bindless descriptor set layout created (binding 0, {} slots)", MAX_BINDLESS_TEXTURES);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Private: create descriptor pool
// ---------------------------------------------------------------------------

Result<bool> BindlessTextureRegistry::create_descriptor_pool() {
    VkDescriptorPoolSize pool_size{};
    pool_size.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = MAX_BINDLESS_TEXTURES;

    VkDescriptorPoolCreateInfo pool_ci{};
    pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_ci.maxSets       = 1;
    pool_ci.poolSizeCount = 1;
    pool_ci.pPoolSizes    = &pool_size;

    VkResult vr = vkCreateDescriptorPool(device_, &pool_ci, nullptr, &descriptor_pool_);
    if (vr != VK_SUCCESS) {
        return Result<bool>::err("vkCreateDescriptorPool (bindless) failed: "
                                 + std::to_string(static_cast<int>(vr)));
    }

    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Private: create static sampler table
// ---------------------------------------------------------------------------

Result<bool> BindlessTextureRegistry::create_samplers() {
    // Derivation: 16 static sampler slots.
    // Slots 0-2: linear  × repeat/clamp/mirror.
    // Slot  3:   linear  × repeat + anisotropy (albedo, most common).
    // Slots 4-6: nearest × repeat/clamp/mirror.
    // Slots 7-15: reserved.
    //
    // All samplers use LINEAR mip filtering for sharpness at distance.

    struct SamplerSpec {
        VkFilter      filter;
        VkSamplerAddressMode address;
        bool          anisotropy;
    };

    const std::array<SamplerSpec, 7> specs{{
        {VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT,          false},
        {VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   false},
        {VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,  false},
        {VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT,           true },
        {VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT,           false},
        {VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   false},
        {VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,  false},
    }};

    for (size_t i = 0; i < specs.size(); ++i) {
        const auto& spec = specs[i];
        VkSamplerCreateInfo ci{};
        ci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        ci.magFilter               = spec.filter;
        ci.minFilter               = spec.filter;
        ci.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        ci.addressModeU            = spec.address;
        ci.addressModeV            = spec.address;
        ci.addressModeW            = spec.address;
        ci.mipLodBias              = 0.0f;
        ci.anisotropyEnable        = spec.anisotropy ? VK_TRUE : VK_FALSE;
        ci.maxAnisotropy           = spec.anisotropy ? 16.0f : 1.0f;
        ci.compareEnable           = VK_FALSE;
        ci.minLod                  = 0.0f;
        ci.maxLod                  = VK_LOD_CLAMP_NONE;
        ci.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        ci.unnormalizedCoordinates = VK_FALSE;

        VkResult vr = vkCreateSampler(device_, &ci, nullptr, &samplers_[i]);
        if (vr != VK_SUCCESS) {
            return Result<bool>::err("vkCreateSampler [" + std::to_string(i) + "] failed: "
                                     + std::to_string(static_cast<int>(vr)));
        }
    }

    spdlog::info("Bindless sampler table created ({} samplers)", specs.size());
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Private: upload sentinel (1×1 magenta RGBA)
// ---------------------------------------------------------------------------

Result<bool> BindlessTextureRegistry::upload_sentinel(VkCommandPool command_pool) {
    // Magenta sentinel: RGBA = (255, 0, 255, 255).
    // Slot 0 is permanently occupied (never enters the free list).
    // A fragment that samples from an unloaded slot will show magenta,
    // making missing-texture bugs visually obvious.
    const uint8_t magenta[4] = {255, 0, 255, 255};

    auto result = upload_texture(magenta, 1u, 1u, command_pool, 0u);
    if (result.is_err()) {
        return Result<bool>::err("Failed to upload bindless sentinel: " + result.error());
    }

    spdlog::info("Bindless sentinel uploaded (1x1 magenta at slot 0)");
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// Private: upload_texture — upload RGBA pixels to a given slot
// ---------------------------------------------------------------------------

Result<TextureHandle> BindlessTextureRegistry::upload_texture(
    const uint8_t* rgba, uint32_t w, uint32_t h,
    VkCommandPool command_pool, uint32_t slot)
{
    VkDeviceSize image_size = static_cast<VkDeviceSize>(w) * h * 4;

    // Staging buffer.
    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation staging_alloc = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo buf_ci{};
        buf_ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_ci.size        = image_size;
        buf_ci.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_ci{};
        alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                       | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo alloc_info{};
        VkResult vr = vmaCreateBuffer(vma_, &buf_ci, &alloc_ci, &staging, &staging_alloc, &alloc_info);
        if (vr != VK_SUCCESS) {
            return Result<TextureHandle>::err("Bindless: staging buffer create failed: "
                                             + std::to_string(static_cast<int>(vr)));
        }
        std::memcpy(alloc_info.pMappedData, rgba, image_size);
    }

    // Device-local image.
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation image_alloc = VK_NULL_HANDLE;
    {
        VkImageCreateInfo img_ci{};
        img_ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img_ci.imageType     = VK_IMAGE_TYPE_2D;
        img_ci.format        = VK_FORMAT_R8G8B8A8_SRGB;
        img_ci.extent        = {w, h, 1u};
        img_ci.mipLevels     = 1u;
        img_ci.arrayLayers   = 1u;
        img_ci.samples       = VK_SAMPLE_COUNT_1_BIT;
        img_ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
        img_ci.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        img_ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo alloc_ci{};
        alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_ci.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        VkResult vr = vmaCreateImage(vma_, &img_ci, &alloc_ci, &image, &image_alloc, nullptr);
        if (vr != VK_SUCCESS) {
            vmaDestroyBuffer(vma_, staging, staging_alloc);
            return Result<TextureHandle>::err("Bindless: image create failed: "
                                             + std::to_string(static_cast<int>(vr)));
        }
    }

    // Transition + copy + transition.
    VkCommandBuffer cmd = begin_single_time_commands(device_, command_pool);

    // UNDEFINED → TRANSFER_DST
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Copy.
    {
        VkBufferImageCopy copy{};
        copy.bufferOffset       = 0;
        copy.bufferRowLength    = 0;
        copy.bufferImageHeight  = 0;
        copy.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount     = 1;
        copy.imageExtent                     = {w, h, 1u};
        vkCmdCopyBufferToImage(cmd, staging, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }

    // TRANSFER_DST → SHADER_READ_ONLY
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    end_single_time_commands(device_, queue_, command_pool, cmd);

    // Clean up staging buffer after GPU completes (end_single_time_commands waits).
    vmaDestroyBuffer(vma_, staging, staging_alloc);

    // Create image view.
    VkImageView view = VK_NULL_HANDLE;
    {
        VkImageViewCreateInfo view_ci{};
        view_ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.image                           = image;
        view_ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        view_ci.format                          = VK_FORMAT_R8G8B8A8_SRGB;
        view_ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_ci.subresourceRange.levelCount     = 1;
        view_ci.subresourceRange.layerCount     = 1;

        VkResult vr = vkCreateImageView(device_, &view_ci, nullptr, &view);
        if (vr != VK_SUCCESS) {
            vmaDestroyImage(vma_, image, image_alloc);
            return Result<TextureHandle>::err("Bindless: image view create failed: "
                                             + std::to_string(static_cast<int>(vr)));
        }
    }

    // Store entry.
    entries_[slot] = TextureEntry{
        TextureHandle::make(slot, slot == 0u ? 0u : slot_alloc_.generation_of(slot)),
        image,
        image_alloc,
        view,
        w,
        h
    };

    // Write descriptor.
    write_descriptor(slot, view, samplers_[0]);

    return Result<TextureHandle>::ok(entries_[slot].handle);
}

// ---------------------------------------------------------------------------
// Private: write_descriptor — update one descriptor array element in-place
// ---------------------------------------------------------------------------

void BindlessTextureRegistry::write_descriptor(uint32_t slot, VkImageView view, VkSampler sampler) {
    VkDescriptorImageInfo img_info{};
    img_info.sampler     = sampler;
    img_info.imageView   = view;
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = descriptor_set_;
    write.dstBinding      = 0;
    write.dstArrayElement = slot;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &img_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

// ---------------------------------------------------------------------------
// Private: destroy_texture_entry
// ---------------------------------------------------------------------------

void BindlessTextureRegistry::destroy_texture_entry(TextureEntry& entry) {
    if (entry.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, entry.view, nullptr);
        entry.view = VK_NULL_HANDLE;
    }
    if (entry.image != VK_NULL_HANDLE) {
        vmaDestroyImage(vma_, entry.image, entry.alloc);
        entry.image = VK_NULL_HANDLE;
        entry.alloc = VK_NULL_HANDLE;
    }
    entry.handle = TextureHandle{};
}

} // namespace odyssey::vulkan
