#pragma once
#include "core/result.h"
#include "vulkan/bindless_slot_allocator.h"
#include "vulkan/device.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace odyssey::vulkan {

/// ---------------------------------------------------------------------------
/// Bindless texture capacity constants (architect condition).
///
/// Derivation:
///   MAX_BINDLESS_TEXTURES = 16384
///     Chosen to give headroom for the lighting-mood-architect's Phase-6+N
///     slot reservations (shadow atlas array, 3D LUT grades, volumetric
///     froxel volume) while staying within the minimum required limit of
///     maxDescriptorSetUpdateAfterBindSampledImages on Vulkan 1.2-capable
///     hardware (RTX 3080 reports 2^20, well above 16384).
///     Phase-6 showcase budget: ~100 materials × ~1 albedo each ≈ 100 slots
///     used; asset-engineer proposed 4096 but architect's 16384 stands per
///     decision record reconciliation note.
///     16-bit TextureHandle slot field limits us to 65535 — 16384 is safe.
///
///   MAX_BINDLESS_SAMPLERS = 16
///     Static sampler variants (one immutable VkSampler per slot):
///       0  linear  + repeat  (most common — albedo maps)
///       1  linear  + clamp
///       2  linear  + mirror
///       3  linear  + anisotropic repeat
///       4  nearest + repeat
///       5  nearest + clamp
///       6  nearest + mirror
///       7..15 reserved for future lighting LUTs, shadow PCF, etc.
///
///   Slot 0 reserved for the 1×1 magenta sentinel (RGBA = 1,0,1,1).
///   Never allocatable by the free list.
///
///   Phase-6+N lighting reservation ranges (documented per lighting condition):
///     Shadow map atlas array : reserve 1 slot    (slot 1, allocated at Phase-7)
///     3D LUT grade textures  : reserve 6 slots   (one per LightingProfile)
///     Volumetric froxel vol  : reserve 1 slot    (3D texture, single slot)
///   These are DOCUMENTATION ONLY in Phase 6 — no slots are pre-allocated,
///   preventing wasted memory while the follow-on is not merged.
/// ---------------------------------------------------------------------------
inline constexpr uint32_t MAX_BINDLESS_TEXTURES = 16384u;
inline constexpr uint32_t MAX_BINDLESS_SAMPLERS = 16u;

/// Error codes for registry operations.
enum class RegistryErr : uint32_t {
    AllocatorFull,          ///< slot table exhausted
    VulkanImageCreateFailed,
    VulkanImageViewFailed,
    VulkanSamplerFailed,
    VulkanDescriptorFailed,
    StagingFailed,
    InvalidHandle,          ///< handle stale or out-of-range
};

std::string registry_err_to_string(RegistryErr e);

/// ---------------------------------------------------------------------------
/// Per-texture GPU-side resources.
/// ---------------------------------------------------------------------------
struct TextureEntry {
    TextureHandle handle;
    VkImage       image      = VK_NULL_HANDLE;
    VmaAllocation alloc      = VK_NULL_HANDLE;
    VkImageView   view       = VK_NULL_HANDLE;
    uint32_t      width      = 0;
    uint32_t      height     = 0;
};

/// ---------------------------------------------------------------------------
/// BindlessTextureRegistry — owns slot allocator, descriptor set, and all
/// resident texture GPU resources.
///
/// Architecture (set=0 bindless, set=1 frame UBOs, push constants per-draw):
///   - One VkDescriptorSetLayout (set=0) with:
///       binding 0: combined-image-sampler array [MAX_BINDLESS_TEXTURES]
///                  UPDATE_AFTER_BIND, PARTIALLY_BOUND, VARIABLE_DESCRIPTOR_COUNT
///   - One VkDescriptorPool with UPDATE_AFTER_BIND_BIT.
///   - One VkDescriptorSet (persists for the engine lifetime).
///
/// Thread safety: NOT thread-safe.  All calls must occur on the main/render
/// thread.  Texture upload stalls are bounded by the staging-buffer copy +
/// fence wait at end-of-frame — no unbounded allocation during gameplay (marty
/// audio condition).
///
/// Dedup: by resolved absolute path.  Two different relative paths that resolve
/// to the same file receive the same handle (3d-modeler condition).
///
/// Hot-reload: caller calls unload(handle) then load(path).  unload() writes
/// the magenta sentinel to the freed slot BEFORE returning it to the free list
/// (asset-engineer condition).  The new load() writes the updated descriptor
/// entry in-place; no rebind required because UPDATE_AFTER_BIND is enabled.
/// ---------------------------------------------------------------------------
class BindlessTextureRegistry {
public:
    BindlessTextureRegistry() = default;
    ~BindlessTextureRegistry();

    // Not copyable (owns GPU resources).
    BindlessTextureRegistry(const BindlessTextureRegistry&) = delete;
    BindlessTextureRegistry& operator=(const BindlessTextureRegistry&) = delete;

    /// Impure: initialize the registry.  Creates descriptor pool + layout + set,
    /// uploads the 1×1 magenta sentinel to slot 0.
    Result<bool> initialize(const DeviceContext& device_ctx,
                            VkCommandPool command_pool);

    /// Impure: destroy all GPU resources.  Safe to call on un-initialized registry.
    void shutdown();

    /// Impure: load a texture from RGBA pixel data.  Dedup by resolved absolute path.
    /// Returns an existing handle if already loaded.
    Result<TextureHandle> load(const std::filesystem::path& abs_path,
                               const uint8_t* rgba_pixels,
                               uint32_t width, uint32_t height,
                               VkCommandPool command_pool);

    /// Impure: unload a texture.  Writes magenta sentinel to the freed slot,
    /// then returns it to the free list.  Removes path from dedup map.
    Result<bool> unload(TextureHandle handle);

    /// Pure: look up an already-loaded texture by path.  Returns invalid handle
    /// if not found.
    TextureHandle find(const std::filesystem::path& abs_path) const;

    /// Pure: the VkDescriptorSetLayout for set=0.  Callers need this for pipeline layout.
    VkDescriptorSetLayout descriptor_set_layout() const { return descriptor_set_layout_; }

    /// Pure: the live VkDescriptorSet for set=0.
    VkDescriptorSet descriptor_set() const { return descriptor_set_; }

    /// Pure: occupancy stats for the CLI / debug overlay.
    uint32_t used_slots()  const { return slot_alloc_.used(); }
    uint32_t free_slots()  const { return slot_alloc_.free_count(); }
    uint32_t capacity()    const { return slot_alloc_.capacity(); }

    /// Pure: handle for the sentinel (slot 0, generation 0).
    static TextureHandle sentinel_handle() { return TextureHandle::make(0u, 0u); }

private:
    // Vulkan device refs (not owned).
    VkDevice      device_ = VK_NULL_HANDLE;
    VkQueue       queue_  = VK_NULL_HANDLE;

    // Descriptor objects.
    VkDescriptorPool      descriptor_pool_       = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorSet       descriptor_set_        = VK_NULL_HANDLE;

    // Samplers (static table — see MAX_BINDLESS_SAMPLERS comment above).
    VkSampler samplers_[MAX_BINDLESS_SAMPLERS] = {};

    // Per-slot texture storage.
    std::vector<TextureEntry> entries_;

    // Path → handle dedup (resolved absolute path key).
    std::unordered_map<std::string, TextureHandle> path_map_;

    // Pure slot allocator.
    BindlessSlotAllocator slot_alloc_{MAX_BINDLESS_TEXTURES};

    // VMA allocator from device context (held by reference, not owned).
    VmaAllocator vma_ = VK_NULL_HANDLE;

    // Internal helpers.
    Result<bool> create_descriptor_layout();
    Result<bool> create_descriptor_pool();
    Result<bool> create_samplers();
    Result<bool> upload_sentinel(VkCommandPool command_pool);
    Result<TextureHandle> upload_texture(const uint8_t* rgba, uint32_t w, uint32_t h,
                                         VkCommandPool command_pool,
                                         uint32_t slot);
    void write_descriptor(uint32_t slot, VkImageView view, VkSampler sampler);
    void destroy_texture_entry(TextureEntry& entry);
};

} // namespace odyssey::vulkan
