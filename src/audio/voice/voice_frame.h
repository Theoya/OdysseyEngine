#pragma once
// VoiceFrame — the full on-wire VOICE_FRAME packet as a pure data struct plus
// its serialize/deserialize pair. Sits above net/protocol.h and is the
// representation that Codec + JitterBuffer + the relay path all agree on.
//
// Wire layout (also described in src/net/protocol.h):
//
//   [PacketHeader 16 B][VoiceSubHeader 8 B][opus_payload N B]
//
// The Opus payload is opaque bytes here — this layer never decodes it, per the
// design doc §6 "Relaying without touching the codec payload".
//
// Mandate #1 (pure/lean): serialize/deserialize are pure; they construct and
// consume byte buffers without touching sockets or global state. The I/O side
// (send/recv) lives in client.cpp / server.cpp.
//
// Mandate #2 (success+failure): deserialize returns Result<VoiceFrame,
// ProtocolError> so every distinct error mode gets a test.
//
// References:
//   - RFC 6716 §2.1.1 — Opus frame sizing (20 ms × 24 kbps ≈ 60 B).
//   - docs/design/proximity_chat_netcode.md §5 — packet format rationale.

#include "core/result.h"
#include "net/protocol.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace odyssey::audio::voice {

// Error codes for voice-frame wire handling. Named rather than stringly-typed
// so callers (relay path, jitter buffer, tests) can branch cleanly.
enum class ProtocolError {
    TruncatedHeader,     // buffer too small to even hold the PacketHeader
    BadProtocolId,       // PacketHeader.protocol_id != PROTOCOL_ID
    BadProtocolVersion,  // caller-supplied version check failed
    WrongPacketType,     // PacketType wasn't VOICE_FRAME
    TruncatedSubHeader,  // header present but < 8 B of voice sub-header
    OversizePayload,     // total packet > MAX_PACKET_SIZE (1200 B)
    ReservedFlagsSet,    // bits 4-7 of flags non-zero (future-proofing gate)
};

std::string to_string(ProtocolError e);

// A fully parsed voice packet. The opus_payload vector may be empty — this is
// valid on the wire only when flags has END_OF_TALK set (a sentinel flush
// from the speaker). Pure-data: no lifetime or ownership beyond the vector.
struct VoiceFrame {
    odyssey::net::PacketHeader    header{};
    odyssey::net::VoiceSubHeader  voice{};
    std::vector<uint8_t>          opus_payload; // opaque codec bytes
};

// Pure: encode a VoiceFrame to wire bytes. Caller has already populated the
// PacketHeader (sequence/ack/ack_bits/type=VOICE_FRAME). Never fragments —
// if total_size > MAX_PACKET_SIZE the returned buffer will still be built
// (caller is responsible for size checks; see test_voice_frame.cpp
// "oversize" case — deserialize is the enforcement point, because a relay
// is what's vulnerable to malicious oversize).
std::vector<uint8_t> serialize_voice_frame(const VoiceFrame& vf);

// Pure: decode wire bytes to a VoiceFrame. Strict validation:
//   - size >= 16 (header)                          else TruncatedHeader
//   - header.protocol_id == PROTOCOL_ID            else BadProtocolId
//   - header.type == VOICE_FRAME                   else WrongPacketType
//   - size >= 16 + 8 (sub-header)                  else TruncatedSubHeader
//   - size <= MAX_PACKET_SIZE                      else OversizePayload
//   - (flags & RESERVED_MASK) == 0                 else ReservedFlagsSet
// Zero-length opus_payload is permitted (END_OF_TALK marker).
Result<VoiceFrame, ProtocolError>
deserialize_voice_frame(const uint8_t* data, size_t size);

} // namespace odyssey::audio::voice
