#include "audio/voice/jitter_buffer.h"

namespace odyssey::audio::voice {

std::string to_string(JitterError e) {
    switch (e) {
        case JitterError::Empty:       return "Empty";
        case JitterError::StalePacket: return "StalePacket";
        case JitterError::Duplicate:   return "Duplicate";
        case JitterError::Flushed:     return "Flushed";
    }
    return "Unknown";
}

void JitterBuffer::reset() {
    for (auto& s : ring_) {
        s.occupied = false;
        s.end_of_talk = false;
        s.sequence = 0;
        s.flags = 0;
        s.payload.clear();
    }
    next_seq_to_play_ = 0;
    armed_ = false;
}

size_t JitterBuffer::depth() const {
    size_t n = 0;
    for (const auto& s : ring_) if (s.occupied) ++n;
    return n;
}

Result<bool, JitterError> JitterBuffer::push(const VoiceFrame& vf) {
    using R = Result<bool, JitterError>;

    const uint16_t seq = vf.voice.sequence;

    // First packet arms the buffer. The cursor starts at seq — this frame is
    // the next thing to play. Subsequent frames position-relative to this.
    if (!armed_) {
        armed_ = true;
        next_seq_to_play_ = seq;
    }

    const int32_t delta = seq_delta(next_seq_to_play_, seq);

    // Late: before the play cursor. Could be a genuine out-of-order arrival
    // (which we can't play because its slot has already been consumed) or a
    // replay attacker. Either way, drop.
    if (delta < 0) {
        // Within the replay window → late-but-real. Beyond it → stale replay.
        // Both paths return Stale; separate reason is logged by caller.
        (void)kReplayWindow;
        return R::err(JitterError::StalePacket);
    }

    // Overflow: frame is too far in the future to fit in a 3-slot ring.
    // Drop-oldest: jump the cursor forward so the incoming frame lands at
    // slot [seq % kSlots]. Anything previously in the ring that would now be
    // "behind" the cursor is abandoned.
    if (delta >= static_cast<int32_t>(kSlots)) {
        // New cursor positions the incoming frame at offset (kSlots - 1) from
        // the cursor — i.e. this frame will be the LAST to play from the ring.
        // That favours fresher audio (drop-oldest semantics).
        const uint16_t new_cursor = static_cast<uint16_t>(seq - (kSlots - 1));
        // Invalidate slots that fall behind the new cursor.
        for (auto& s : ring_) {
            if (s.occupied) {
                const int32_t d = seq_delta(new_cursor, s.sequence);
                if (d < 0 || d >= static_cast<int32_t>(kSlots)) {
                    s.occupied = false;
                    s.end_of_talk = false;
                    s.payload.clear();
                }
            }
        }
        next_seq_to_play_ = new_cursor;
    }

    // Insert at the modular slot index. If a frame is already there with
    // the same sequence, that's a duplicate — drop. If it's a DIFFERENT
    // sequence (possible after wraparound if the ring is very sparse), the
    // incoming is fresher by the delta check above, so it replaces.
    Slot& slot = ring_[seq % kSlots];
    if (slot.occupied && slot.sequence == seq) {
        return R::err(JitterError::Duplicate);
    }
    slot.occupied = true;
    slot.sequence = seq;
    slot.flags = vf.voice.flags;
    slot.end_of_talk = (vf.voice.flags & odyssey::net::voice_flags::END_OF_TALK) != 0;
    slot.payload = vf.opus_payload;

    return R::ok(true);
}

Result<PopResult, JitterError> JitterBuffer::pop() {
    using R = Result<PopResult, JitterError>;

    if (!armed_) {
        return R::err(JitterError::Empty);
    }

    Slot& slot = ring_[next_seq_to_play_ % kSlots];
    PopResult out;
    out.sequence = next_seq_to_play_;

    if (slot.occupied && slot.sequence == next_seq_to_play_) {
        out.flags = slot.flags;

        if (slot.end_of_talk) {
            // END_OF_TALK is a sentinel; flush everything downstream.
            reset();
            out.kind = PopKind::EndOfTalk;
            return R::ok(std::move(out));
        }

        out.kind = PopKind::HasFrame;
        out.opus_payload = std::move(slot.payload);

        // Free the slot before advancing so a wraparound doesn't alias stale
        // memory at the same modular index.
        slot.occupied = false;
        slot.end_of_talk = false;
        slot.sequence = 0;
        slot.flags = 0;
        slot.payload.clear();
    } else {
        // Gap: slot is empty or holds a different sequence (from a prior
        // wraparound and then cleared). Caller invokes Codec::plc().
        out.kind = PopKind::NeedsPLC;
    }

    // u16 increment naturally wraps 65535 → 0. This is the whole point of
    // using u16 sequences — no explicit modulo, no branch.
    ++next_seq_to_play_;
    return R::ok(std::move(out));
}

} // namespace odyssey::audio::voice
