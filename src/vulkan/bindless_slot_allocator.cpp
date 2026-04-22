#include "vulkan/bindless_slot_allocator.h"

#include <algorithm>
#include <cassert>

namespace odyssey::vulkan {

std::string alloc_err_to_string(AllocErr e) {
    switch (e) {
        case AllocErr::TableFull:          return "AllocErr::TableFull";
        case AllocErr::DoubleFree:         return "AllocErr::DoubleFree";
        case AllocErr::OutOfRange:         return "AllocErr::OutOfRange";
        case AllocErr::GenerationMismatch: return "AllocErr::GenerationMismatch";
    }
    return "AllocErr::Unknown";
}

// ---------------------------------------------------------------------------
// BindlessSlotAllocator
// ---------------------------------------------------------------------------

BindlessSlotAllocator::BindlessSlotAllocator(uint32_t capacity)
    : capacity_(std::min(capacity, static_cast<uint32_t>(TextureHandle::SLOT_MASK)))
    , generations_(capacity_, 0u)
    , occupied_(capacity_, false)
{
    // Slot 0 is permanently reserved for the magenta sentinel texture.
    // We mark it occupied so alloc() can never hand it out.
    occupied_[0] = true;

    // Pre-fill free list with slots [1, capacity-1] in reverse order so that
    // the first alloc() returns slot 1 (smallest index = LIFO off the back).
    //
    // Derivation: we want alloc() to pop from back() for O(1) amortized cost,
    // and we want deterministic ordering (lowest index first) so tests are
    // stable.  Filling in reverse gives us that: pop_back returns capacity-1
    // last ... actually we want slot 1 first so fill ascending so back=cap-1
    // is popped last.  Then the first pop_back gives capacity-1 which is
    // large.  Instead fill descending [capacity-1 ... 1] so back = 1 and
    // pop_back gives 1 on the first alloc.
    free_list_.reserve(capacity_ - 1);
    for (uint32_t i = capacity_ - 1; i >= 1; --i) {
        free_list_.push_back(i);
    }
    // free_list_.back() == 1 now, so alloc() returns slot 1 first.
}

Result<TextureHandle, AllocErr> BindlessSlotAllocator::alloc() {
    if (free_list_.empty()) {
        return Result<TextureHandle, AllocErr>::err(AllocErr::TableFull);
    }

    uint32_t slot = free_list_.back();
    free_list_.pop_back();

    occupied_[slot] = true;
    // Generation is NOT incremented on alloc — it was incremented on the
    // previous free() (or starts at 0 for a brand-new slot).  The handle
    // encodes the current generation so the caller can detect stale handles.
    return Result<TextureHandle, AllocErr>::ok(
        TextureHandle::make(slot, generations_[slot]));
}

Result<bool, AllocErr> BindlessSlotAllocator::free(TextureHandle handle) {
    uint32_t slot = handle.slot();

    // Range check (slot 0 is the sentinel — it must never be freed).
    if (slot == 0 || slot >= capacity_) {
        return Result<bool, AllocErr>::err(AllocErr::OutOfRange);
    }

    // Double-free detection: if the slot is not occupied it's already free.
    if (!occupied_[slot]) {
        return Result<bool, AllocErr>::err(AllocErr::DoubleFree);
    }

    // Generation mismatch: handle is stale (alloc'd in a previous life).
    if (handle.generation() != generations_[slot]) {
        return Result<bool, AllocErr>::err(AllocErr::GenerationMismatch);
    }

    // Commit the free.
    occupied_[slot] = false;

    // Advance generation so any surviving handles to this slot become stale
    // immediately.  Wrap within 16 bits (the handle field width).
    generations_[slot] = (generations_[slot] + 1u) & 0xFFFFu;

    free_list_.push_back(slot);
    return Result<bool, AllocErr>::ok(true);
}

} // namespace odyssey::vulkan
