#pragma once
#include "core/result.h"
#include <cstdint>
#include <vector>
#include <string>

namespace odyssey::vulkan {

/// ---------------------------------------------------------------------------
/// TextureHandle — opaque handle for a bindless texture slot.
///
/// Carries both a slot index and a generation counter so callers can detect
/// use-after-free / reload-after-unload at the same path (the most common
/// bindless bug per asset-engineer's concern #1 in the decision record).
///
/// Layout: upper 16 bits = generation, lower 16 bits = slot index.
/// Total: 32 bits, safe to pass as a push-constant uint32_t.
/// ---------------------------------------------------------------------------
struct TextureHandle {
    static constexpr uint32_t INVALID = UINT32_MAX;
    static constexpr uint32_t SLOT_MASK       = 0x0000FFFFu; // bits 0-15
    static constexpr uint32_t GENERATION_MASK = 0xFFFF0000u; // bits 16-31
    static constexpr uint32_t GENERATION_SHIFT = 16u;

    uint32_t raw = INVALID;

    bool is_valid()  const { return raw != INVALID; }
    uint32_t slot()  const { return raw & SLOT_MASK; }
    uint32_t generation() const { return (raw & GENERATION_MASK) >> GENERATION_SHIFT; }

    /// Slot 0 is the magenta sentinel — never allocatable from the free list.
    bool is_sentinel() const { return slot() == 0 && is_valid(); }

    bool operator==(const TextureHandle& o) const { return raw == o.raw; }
    bool operator!=(const TextureHandle& o) const { return raw != o.raw; }

    static TextureHandle make(uint32_t slot, uint32_t generation) {
        TextureHandle h;
        h.raw = ((generation << GENERATION_SHIFT) & GENERATION_MASK) | (slot & SLOT_MASK);
        return h;
    }
};

/// Error codes for allocation operations.
enum class AllocErr : uint32_t {
    TableFull,       ///< All slots exhausted
    DoubleFree,      ///< slot freed while already in the free list
    OutOfRange,      ///< slot index >= table capacity
    GenerationMismatch, ///< generation in handle != generation stored in table
};

std::string alloc_err_to_string(AllocErr e);

/// ---------------------------------------------------------------------------
/// BindlessSlotAllocator — pure free-list over a fixed slot table.
///
/// Design (M3 first-principles derivation):
///   - Slot 0 is permanently reserved for the 1×1 magenta sentinel texture.
///     It is pre-marked occupied at construction and never enters the free list.
///   - All other slots [1, capacity-1] start in the free list.
///   - alloc() pops the head of the free list and increments that slot's
///     generation counter so handles from a previous life detect stale access.
///   - free(handle) validates: slot in range, generation matches, slot not
///     already free. On success it increments generation again (so future
///     alloc of the same slot produces a new generation) and pushes to free list.
///   - No Vulkan or GPU dependencies. Pure arithmetic; can be exercised in
///     unit tests without a device.
///
/// Slot capacity constraint: 16-bit slot field limits max capacity to 65535.
/// Actual cap = min(capacity, 65535).  MAX_BINDLESS_TEXTURES = 16384 is
/// well within this range.
/// ---------------------------------------------------------------------------
class BindlessSlotAllocator {
public:
    /// Construct with a fixed capacity.  Slot 0 is immediately reserved.
    explicit BindlessSlotAllocator(uint32_t capacity);

    /// Not copyable (owns free-list state).
    BindlessSlotAllocator(const BindlessSlotAllocator&) = delete;
    BindlessSlotAllocator& operator=(const BindlessSlotAllocator&) = delete;

    /// Move is fine.
    BindlessSlotAllocator(BindlessSlotAllocator&&) = default;
    BindlessSlotAllocator& operator=(BindlessSlotAllocator&&) = default;

    /// Pure query: total slot capacity.
    uint32_t capacity() const { return capacity_; }

    /// Pure query: number of slots currently in use (including slot 0 sentinel).
    uint32_t used() const { return capacity_ - static_cast<uint32_t>(free_list_.size()); }

    /// Pure query: number of free allocatable slots (not counting slot 0).
    uint32_t free_count() const { return static_cast<uint32_t>(free_list_.size()); }

    /// Allocate a slot and return a fresh TextureHandle.
    /// Fails with AllocErr::TableFull when free_list_ is empty.
    Result<TextureHandle, AllocErr> alloc();

    /// Release a slot.  Validates range, generation, and double-free.
    /// On success: increments generation of the slot and pushes index to free list.
    /// Returns Result<bool, AllocErr>: ok(true) on success.
    Result<bool, AllocErr> free(TextureHandle handle);

    /// Return the current generation for a slot (for registry bookkeeping).
    /// No range check — caller must guard.
    uint32_t generation_of(uint32_t slot) const { return generations_[slot]; }

    /// True if the slot is currently allocated (i.e., not in the free list).
    /// Slot 0 reports occupied = true always (sentinel).
    bool is_occupied(uint32_t slot) const { return occupied_[slot]; }

private:
    uint32_t capacity_;
    std::vector<uint32_t> generations_; ///< per-slot generation counter
    std::vector<bool>     occupied_;    ///< true = slot is allocated
    std::vector<uint32_t> free_list_;   ///< stack (back = next to alloc)
};

} // namespace odyssey::vulkan
