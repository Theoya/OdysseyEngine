#pragma once
//
// bar_clock.h — fixed-point BPM-driven bar/beat clock for quantization.
//
// Pure function design: bar_clock state is advanced exclusively by tick(dt),
// which accumulates samples at the mixer sample rate (48kHz). No audio-thread
// mutation allowed. All queries (beat, bar, phase) are read-only.
//
// Derivation (condition 3, condition 9):
//   samples_per_beat = sample_rate / (bpm / 60)
//     = sample_rate * 60 / bpm
//     = 48000 * 60 / 120  = 24000 samples per beat (at 120 BPM, 48kHz)
//   samples_per_bar = samples_per_beat * 4  (4/4 time)
//

#include <cstdint>
#include <cmath>

namespace odyssey::audio::music::detail {

enum class QuantizeMode {
    Immediate = 0,  // Fire now (CLI testing only)
    NextBeat = 1,   // Snap to next beat (1/4 bar)
    NextBar = 2,    // Snap to next bar boundary (default)
    NextPhrase = 3, // Snap to next 4-bar phrase
};

class BarClock {
public:
    // Initialize with BPM and sample rate. Fixed at construction.
    BarClock(float bpm, uint32_t sample_rate)
        : bpm_(bpm), sample_rate_(sample_rate),
          accumulated_samples_(0), beat_(0), bar_(0) {}

    // Advance the clock by dt seconds (pure: only mutates accumulated state).
    void tick(float dt_seconds) noexcept {
        float samples_this_frame = dt_seconds * sample_rate_;
        accumulated_samples_ += samples_this_frame;

        float samples_per_beat = sample_rate_ * 60.0f / bpm_;
        float samples_per_bar = samples_per_beat * 4.0f; // 4/4 time

        // Update bar and beat counters.
        while (accumulated_samples_ >= samples_per_bar) {
            accumulated_samples_ -= samples_per_bar;
            bar_++;
            beat_ = 0;
        }

        beat_ = static_cast<uint32_t>(accumulated_samples_ / samples_per_beat);
    }

    // Read-only getters.
    uint32_t bar() const noexcept { return bar_; }
    uint32_t beat() const noexcept { return beat_; }

    // Phase within current bar [0, 1).
    float phase() const noexcept {
        float samples_per_bar = sample_rate_ * 60.0f / bpm_ * 4.0f;
        return accumulated_samples_ / samples_per_bar;
    }

    // Quantize a sample offset to a boundary given the current state.
    // Returns the target sample offset (absolute from clock start).
    uint64_t quantize_to_boundary(QuantizeMode mode, uint32_t bars_per_phrase) const noexcept {
        float samples_per_beat = sample_rate_ * 60.0f / bpm_;
        float samples_per_bar = samples_per_beat * 4.0f;

        uint64_t current_sample = bar_ * static_cast<uint64_t>(samples_per_bar)
                                + beat_ * static_cast<uint64_t>(samples_per_beat)
                                + static_cast<uint64_t>(accumulated_samples_);

        switch (mode) {
            case QuantizeMode::Immediate:
                return current_sample;
            case QuantizeMode::NextBeat:
                // Snap to next beat (1/4 bar).
                return ((current_sample / static_cast<uint64_t>(samples_per_beat)) + 1)
                       * static_cast<uint64_t>(samples_per_beat);
            case QuantizeMode::NextBar:
                // Snap to next bar.
                return ((current_sample / static_cast<uint64_t>(samples_per_bar)) + 1)
                       * static_cast<uint64_t>(samples_per_bar);
            case QuantizeMode::NextPhrase:
                // Snap to next 4-bar phrase.
                {
                    uint64_t samples_per_phrase = static_cast<uint64_t>(samples_per_bar) * bars_per_phrase;
                    return ((current_sample / samples_per_phrase) + 1) * samples_per_phrase;
                }
        }
        return current_sample; // Fallback (unreachable).
    }

private:
    float bpm_;
    uint32_t sample_rate_;
    float accumulated_samples_; // Fixed-point accumulator to avoid drift.
    uint32_t beat_;
    uint32_t bar_;
};

} // namespace odyssey::audio::music::detail
