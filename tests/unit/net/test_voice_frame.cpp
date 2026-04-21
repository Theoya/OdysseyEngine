// VOICE_FRAME serialize / deserialize unit tests.
//
// Mandate #2: every Result<T,E>-returning function gets success AND failure
// coverage. VoiceFrame's deserialize is the validation gate between wire and
// relay — every error path must be hit explicitly.

#include <gtest/gtest.h>

#include "audio/voice/voice_frame.h"
#include "net/protocol.h"

#include <cstring>
#include <vector>

using namespace odyssey;
using namespace odyssey::audio::voice;
using odyssey::net::PROTOCOL_ID;
using odyssey::net::PROTOCOL_VERSION;
using odyssey::net::PacketType;

namespace {

// Build a valid VoiceFrame with a given opus payload size.
VoiceFrame make_frame(std::vector<uint8_t> payload = {1, 2, 3, 4, 5}) {
    VoiceFrame vf;
    vf.header.protocol_id = PROTOCOL_ID;
    vf.header.sequence = 42;
    vf.header.ack = 10;
    vf.header.ack_bits = 0xDEADBEEF;
    vf.header.type = PacketType::VOICE_FRAME;
    vf.voice.speaker_entity_id = 7;
    vf.voice.sequence = 1234;
    vf.voice.frame_ms = 20;
    vf.voice.flags = odyssey::net::voice_flags::VAD_ACTIVE |
                     odyssey::net::voice_flags::PTT_HELD;
    vf.opus_payload = std::move(payload);
    return vf;
}

} // namespace

TEST(VoiceFrameSerialize, HappyPathRoundTrip) {
    VoiceFrame in = make_frame({0xAA, 0xBB, 0xCC, 0xDD});
    auto bytes = serialize_voice_frame(in);

    // 16 B header + 8 B sub-header + 4 B payload = 28 B.
    EXPECT_EQ(bytes.size(), 16u + 8u + 4u);

    auto parsed = deserialize_voice_frame(bytes.data(), bytes.size());
    ASSERT_TRUE(parsed.is_ok()) << to_string(parsed.error());

    const auto& out = parsed.value();
    EXPECT_EQ(out.header.protocol_id, PROTOCOL_ID);
    EXPECT_EQ(out.header.sequence, 42);
    EXPECT_EQ(out.header.type, PacketType::VOICE_FRAME);
    EXPECT_EQ(out.voice.speaker_entity_id, 7u);
    EXPECT_EQ(out.voice.sequence, 1234);
    EXPECT_EQ(out.voice.frame_ms, 20);
    EXPECT_EQ(out.voice.flags, in.voice.flags);
    ASSERT_EQ(out.opus_payload.size(), 4u);
    EXPECT_EQ(out.opus_payload[0], 0xAA);
    EXPECT_EQ(out.opus_payload[3], 0xDD);
}

TEST(VoiceFrameSerialize, ZeroLengthPayloadAllowed) {
    // END_OF_TALK frames legitimately carry no Opus bytes.
    VoiceFrame in = make_frame({});
    in.voice.flags = odyssey::net::voice_flags::END_OF_TALK;
    auto bytes = serialize_voice_frame(in);
    EXPECT_EQ(bytes.size(), 16u + 8u);

    auto parsed = deserialize_voice_frame(bytes.data(), bytes.size());
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_TRUE(parsed.value().opus_payload.empty());
    EXPECT_NE(parsed.value().voice.flags & odyssey::net::voice_flags::END_OF_TALK, 0);
}

TEST(VoiceFrameDeserialize, TruncatedBelowHeader) {
    uint8_t buf[8] = {};
    auto r = deserialize_voice_frame(buf, sizeof(buf));
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), ProtocolError::TruncatedHeader);
}

TEST(VoiceFrameDeserialize, BadProtocolId) {
    VoiceFrame in = make_frame();
    auto bytes = serialize_voice_frame(in);
    // Corrupt the 4-byte protocol id (LE at offset 0).
    bytes[0] = 0x00;
    bytes[1] = 0x00;
    bytes[2] = 0x00;
    bytes[3] = 0x00;

    auto r = deserialize_voice_frame(bytes.data(), bytes.size());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), ProtocolError::BadProtocolId);
}

TEST(VoiceFrameDeserialize, WrongPacketType) {
    VoiceFrame in = make_frame();
    in.header.type = PacketType::INPUT;
    auto bytes = serialize_voice_frame(in);

    auto r = deserialize_voice_frame(bytes.data(), bytes.size());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), ProtocolError::WrongPacketType);
}

TEST(VoiceFrameDeserialize, TruncatedSubHeader) {
    // Header valid (16 B) but sub-header (8 B) missing — legal-looking
    // PacketHeader followed by nothing.
    VoiceFrame in = make_frame();
    auto bytes = serialize_voice_frame(in);
    bytes.resize(20); // keep 16 B header + 4 B of sub-header (short by 4)

    auto r = deserialize_voice_frame(bytes.data(), bytes.size());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), ProtocolError::TruncatedSubHeader);
}

TEST(VoiceFrameDeserialize, OversizePayloadRejected) {
    // Simulate a malicious peer sending > MAX_PACKET_SIZE. We build a buffer
    // of exactly MAX_PACKET_SIZE + 1 and expect OversizePayload.
    std::vector<uint8_t> buf(odyssey::net::MAX_PACKET_SIZE + 1, 0x00);
    auto r = deserialize_voice_frame(buf.data(), buf.size());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), ProtocolError::OversizePayload);
}

TEST(VoiceFrameDeserialize, ReservedFlagsRejected) {
    VoiceFrame in = make_frame();
    in.voice.flags = 0b11000000; // bits 6 + 7 are reserved in v2
    auto bytes = serialize_voice_frame(in);

    auto r = deserialize_voice_frame(bytes.data(), bytes.size());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), ProtocolError::ReservedFlagsSet);
}

TEST(VoiceFrameSerialize, ByteLayoutMatchesSpec) {
    // Whitebox: byte offsets must match the spec comment in protocol.h so the
    // wire is stable across engine versions. speaker_entity_id sits at offset
    // 16 (after the 16-byte header), sequence at 20, frame_ms at 22, flags at 23.
    VoiceFrame in = make_frame({});
    in.voice.speaker_entity_id = 0x11223344;
    in.voice.sequence = 0x55AA;
    in.voice.frame_ms = 20;
    in.voice.flags = 0x0B;

    auto b = serialize_voice_frame(in);
    ASSERT_GE(b.size(), 24u);
    // speaker_entity_id — little endian u32 at offset 16
    EXPECT_EQ(b[16], 0x44);
    EXPECT_EQ(b[17], 0x33);
    EXPECT_EQ(b[18], 0x22);
    EXPECT_EQ(b[19], 0x11);
    // sequence — u16 at offset 20
    EXPECT_EQ(b[20], 0xAA);
    EXPECT_EQ(b[21], 0x55);
    // frame_ms at 22
    EXPECT_EQ(b[22], 20);
    // flags at 23
    EXPECT_EQ(b[23], 0x0B);
}

TEST(VoiceFrameConstants, ProtocolVersionIsTwo) {
    // Triplet lock: bumping PROTOCOL_VERSION must trip this test so a future
    // edit to the constant without updating the design docs is caught.
    EXPECT_EQ(PROTOCOL_VERSION, 2u);
}
