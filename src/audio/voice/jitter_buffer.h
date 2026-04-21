#pragma once
// JitterBuffer — 3-slot ring, drop-oldest, PLC on gap.
//
// Design (docs/design/proximity_chat_netcode.md §7):
//   - 3 slots × 20 ms = 60 ms of smoothing latency. Under the 150 ms p50
//     mouth-to-ear budget, and enough to absorb 2 consecutive losses plus
//     ordinary wifi jitter.
//   - Drop-oldest on overflow. Voice is a strict monotonic time series;
//     playing 60 ms of stale speech after fresh speech sounds like a glitch.
//     Mumble / Discord / RTP all converge on this.
//   - u16 sequence wraparound handled via signed delta: any distance ≤ 32768
//     forward is "future", ≥ 32769 is "past". Identical to the classic RFC
//     3550 §6.4.4 modular-sequence comparator.
//   - END_OF_TALK flushes. Speaker just released PTT → drop queued speech
//     rather than play a 60 ms tail.
//
// Pure where possible (Mandate #1): push() and pop() work on internal state
// but touch no sockets, no OS. The decoder is owned outside; JitterBuffer
// returns raw payload bytes and a "needs_plc" signal, and the caller routes
// to Codec::decode / Codec::plc. This keeps JitterBuffer testable without
// spinning up an Opus decoder.
//
// Mandate #2: push and pop return Result so every error mode is test-covered.

#include "core/result.h"
#include "audio/voice/voice_frame.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace odyssey::audio::voice {

enum class JitterError {
    Empty,              // pop() called when there's nothing armed yet
    StalePacket,        // push()ed frame is older than the play cursor by > window
    Duplicate,          // push()ed frame matches a slot we already hold
    Flushed,            // END_OF_TALK consumed; caller should treat like disarm
};

std::string to_string(JitterError e);

// What pop() produces. Three distinct states:
//   HasFrame  — decode opus_payload as a real frame.
//   NeedsPLC  — slot was empty; caller invokes Codec::plc().
//   EndOfTalk — sentinel END_OF_TALK was at the cursor; flush upstream.
enum class PopKind : uint8_t {
    HasFrame,
    NeedsPLC,
    EndOfTalk,
};

struct PopResult {
    PopKind kind = PopKind::NeedsPLC;
    uint16_t sequence = 0;                  // which seq we're playing out
    std::vector<uint8_t> opus_payload;      // populated iff kind==HasFrame
    uint8_t flags = 0;                      // flag bits from the original frame
};

class JitterBuffer {
public:
    static constexpr size_t kSlots = 3;

    JitterBuffer() { reset(); }

    // Hard reset — disarm, drop everything.
    void reset();

    // Push a freshly received voice frame into the ring. Returns:
    //   Ok(true)   — accepted, slot populated (or armed the buffer)
    //   Err(Stale) — frame is too old (replay / severe reordering)
    //   Err(Dup)   — this exact sequence is already in the ring
    //
    // Note on arming: the FIRST accepted frame seeds next_seq_to_play_. Until
    // that happens the buffer is "unarmed" — pop() returns Err(Empty).
    Result<bool, JitterError> push(const VoiceFrame& vf);

    // Produce the next slot to feed to the decoder. Advances the play cursor
    // regardless of presence; a missing slot becomes PopKind::NeedsPLC.
    Result<PopResult, JitterError> pop();

    // Accessors for diagnostics (net-stats-dump hook).
    bool armed() const { return armed_; }
    uint16_t next_seq_to_play() const { return next_seq_to_play_; }
    size_t depth() const;

private:
    struct Slot {
        bool occupied = false;
        bool end_of_talk = false;
        uint16_t sequence = 0;
        uint8_t flags = 0;
        std::vector<uint8_t> payload;
    };

    // Signed distance b - a on the u16 sequence ring. Positive = b is ahead
    // of a, negative = b is behind. Matches RFC 3550 §6.4.4.
    static int32_t seq_delta(uint16_t a, uint16_t b) {
        return static_cast<int32_t>(static_cast<int16_t>(b - a));
    }

    std::array<Slot, kSlots> ring_{};
    uint16_t next_seq_to_play_ = 0;
    bool armed_ = false;

    // Replay-attack window. Frames older than the cursor by MORE than this
    // are dropped as StalePacket. Matches design doc §8 "frames more than
    // 32 units old are dropped as replays".
    static constexpr int32_t kReplayWindow = 32;
};

} // namespace odyssey::audio::voice
