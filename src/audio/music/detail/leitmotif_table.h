#pragma once
//
// leitmotif_table.h — O(1) lookup table for leitmotif definitions.
//
// Condition 35: Leitmotif table lookup is O(1) via unordered_map.
// Condition 25: Leitmotif fields: id, source_clip, emotional_register,
//   permitted_contexts, min_reentry_bars.
// Condition 5: Transformations (augmentation, instrumentation, reharmonization)
//   as first-class schema elements.
//

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace odyssey::audio::music::detail {

// Transformation variant: augmentation in semitones.
struct AugmentationTransform {
    int16_t interval_semitones = 0;
};

// Transformation variant: instrument swap.
struct InstrumentationTransform {
    std::string swap_from;
    std::string swap_to;
};

// Transformation variant: harmonic motion.
struct ReharmonizationTransform {
    std::string root_motion;
};

// Leitmotif definition.
struct LeitmotifDef {
    uint32_t id = 0;
    std::string source_clip;           // Path to audio or animation asset.
    std::string emotional_register;    // triumphant|ominous|tender|heroic|wistful|playful|neutral
    std::string permitted_contexts;    // Comma-separated: combat|exploration|dialogue|transition|ambient
    uint16_t min_reentry_bars = 4;     // Cooldown before repeat.

    // Transformations (optional, defaults to identity if absent).
    AugmentationTransform augmentation;
    InstrumentationTransform instrumentation;
    ReharmonizationTransform reharmonization;
    bool has_augmentation = false;
    bool has_instrumentation = false;
    bool has_reharmonization = false;
};

// Leitmotif table: O(1) lookup by stable ID.
class LeitmotifTable {
public:
    // Insert or update a leitmotif.
    void insert(const LeitmotifDef& def) noexcept {
        table_[def.id] = def;
    }

    // Look up by ID. Returns nullptr if not found.
    const LeitmotifDef* lookup(uint32_t id) const noexcept {
        auto it = table_.find(id);
        return it != table_.end() ? &it->second : nullptr;
    }

    // Clear the table.
    void clear() noexcept {
        table_.clear();
    }

    // Count of leitmotifs.
    size_t size() const noexcept {
        return table_.size();
    }

private:
    std::unordered_map<uint32_t, LeitmotifDef> table_;
};

} // namespace odyssey::audio::music::detail
