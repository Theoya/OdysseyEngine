// JitterBuffer tests — in-order, out-of-order, duplicate, gap→PLC,
// END_OF_TALK flush, u16 sequence wraparound, stale-sequence replay drop.
// Covers every branch of push() and pop() per Mandate #2.

#include <gtest/gtest.h>

#include "audio/voice/jitter_buffer.h"
#include "audio/voice/voice_frame.h"
#include "net/protocol.h"

using namespace odyssey;
using namespace odyssey::audio::voice;
using odyssey::net::voice_flags::END_OF_TALK;

namespace {

VoiceFrame frame(uint16_t seq, std::vector<uint8_t> payload = {0x10}, uint8_t flags = 0) {
    VoiceFrame vf;
    vf.header.protocol_id = odyssey::net::PROTOCOL_ID;
    vf.header.type = odyssey::net::PacketType::VOICE_FRAME;
    vf.voice.sequence = seq;
    vf.voice.frame_ms = 20;
    vf.voice.flags = flags;
    vf.opus_payload = std::move(payload);
    return vf;
}

} // namespace

TEST(JitterBuffer, StartsEmptyPopFails) {
    JitterBuffer jb;
    auto r = jb.pop();
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), JitterError::Empty);
}

TEST(JitterBuffer, InOrderFlow) {
    JitterBuffer jb;
    ASSERT_TRUE(jb.push(frame(100, {0xAA})).is_ok());
    ASSERT_TRUE(jb.push(frame(101, {0xBB})).is_ok());
    ASSERT_TRUE(jb.push(frame(102, {0xCC})).is_ok());

    auto r1 = jb.pop();
    ASSERT_TRUE(r1.is_ok());
    EXPECT_EQ(r1.value().kind, PopKind::HasFrame);
    EXPECT_EQ(r1.value().sequence, 100);
    EXPECT_EQ(r1.value().opus_payload[0], 0xAA);

    auto r2 = jb.pop();
    ASSERT_TRUE(r2.is_ok());
    EXPECT_EQ(r2.value().opus_payload[0], 0xBB);

    auto r3 = jb.pop();
    ASSERT_TRUE(r3.is_ok());
    EXPECT_EQ(r3.value().opus_payload[0], 0xCC);

    // Next pop is a gap → NeedsPLC.
    auto r4 = jb.pop();
    ASSERT_TRUE(r4.is_ok());
    EXPECT_EQ(r4.value().kind, PopKind::NeedsPLC);
    EXPECT_EQ(r4.value().sequence, 103);
}

TEST(JitterBuffer, OutOfOrderInsertedByModularIndex) {
    JitterBuffer jb;
    // Arm at 100, then receive 102 before 101.
    ASSERT_TRUE(jb.push(frame(100, {0xA0})).is_ok());
    ASSERT_TRUE(jb.push(frame(102, {0xA2})).is_ok());
    ASSERT_TRUE(jb.push(frame(101, {0xA1})).is_ok());

    // Play order must be 100, 101, 102.
    auto r1 = jb.pop(); ASSERT_TRUE(r1.is_ok());
    EXPECT_EQ(r1.value().opus_payload[0], 0xA0);
    auto r2 = jb.pop(); ASSERT_TRUE(r2.is_ok());
    EXPECT_EQ(r2.value().opus_payload[0], 0xA1);
    auto r3 = jb.pop(); ASSERT_TRUE(r3.is_ok());
    EXPECT_EQ(r3.value().opus_payload[0], 0xA2);
}

TEST(JitterBuffer, DuplicatePushRejected) {
    JitterBuffer jb;
    ASSERT_TRUE(jb.push(frame(50)).is_ok());
    auto dup = jb.push(frame(50));
    ASSERT_TRUE(dup.is_err());
    EXPECT_EQ(dup.error(), JitterError::Duplicate);
}

TEST(JitterBuffer, GapTriggersPLCSignal) {
    JitterBuffer jb;
    ASSERT_TRUE(jb.push(frame(10, {0x01})).is_ok());
    // Skip 11; push 12.
    ASSERT_TRUE(jb.push(frame(12, {0x03})).is_ok());

    auto r10 = jb.pop(); ASSERT_TRUE(r10.is_ok());
    EXPECT_EQ(r10.value().kind, PopKind::HasFrame);

    auto r11 = jb.pop(); ASSERT_TRUE(r11.is_ok());
    EXPECT_EQ(r11.value().kind, PopKind::NeedsPLC); // gap — caller invokes Codec::plc
    EXPECT_EQ(r11.value().sequence, 11);

    auto r12 = jb.pop(); ASSERT_TRUE(r12.is_ok());
    EXPECT_EQ(r12.value().kind, PopKind::HasFrame);
}

TEST(JitterBuffer, EndOfTalkFlushes) {
    JitterBuffer jb;
    ASSERT_TRUE(jb.push(frame(200, {0x01})).is_ok());
    ASSERT_TRUE(jb.push(frame(201, {0x02})).is_ok());
    // EoT at 202 — non-empty payload here is fine; the flag is what matters.
    ASSERT_TRUE(jb.push(frame(202, {0x03}, END_OF_TALK)).is_ok());

    // Normal plays...
    auto r0 = jb.pop(); ASSERT_TRUE(r0.is_ok());
    EXPECT_EQ(r0.value().kind, PopKind::HasFrame);
    auto r1 = jb.pop(); ASSERT_TRUE(r1.is_ok());
    EXPECT_EQ(r1.value().kind, PopKind::HasFrame);

    // EoT pop returns EndOfTalk AND resets the buffer.
    auto eot = jb.pop(); ASSERT_TRUE(eot.is_ok());
    EXPECT_EQ(eot.value().kind, PopKind::EndOfTalk);
    EXPECT_FALSE(jb.armed());

    // Post-flush: pop is Empty again.
    auto after = jb.pop();
    ASSERT_TRUE(after.is_err());
    EXPECT_EQ(after.error(), JitterError::Empty);
}

TEST(JitterBuffer, U16WraparoundInOrder) {
    JitterBuffer jb;
    // Arm right before the wrap boundary.
    ASSERT_TRUE(jb.push(frame(65534)).is_ok());
    ASSERT_TRUE(jb.push(frame(65535)).is_ok());
    ASSERT_TRUE(jb.push(frame(0)).is_ok()); // wrap

    auto a = jb.pop(); ASSERT_TRUE(a.is_ok());
    EXPECT_EQ(a.value().sequence, 65534);
    auto b = jb.pop(); ASSERT_TRUE(b.is_ok());
    EXPECT_EQ(b.value().sequence, 65535);
    auto c = jb.pop(); ASSERT_TRUE(c.is_ok());
    EXPECT_EQ(c.value().sequence, 0);
    EXPECT_EQ(c.value().kind, PopKind::HasFrame);
}

TEST(JitterBuffer, StaleReplayDropped) {
    JitterBuffer jb;
    ASSERT_TRUE(jb.push(frame(500)).is_ok());
    // Consume to advance the cursor to 501.
    (void)jb.pop();

    // A late arrival for seq 499 (one behind the cursor) is stale.
    auto late = jb.push(frame(499));
    ASSERT_TRUE(late.is_err());
    EXPECT_EQ(late.error(), JitterError::StalePacket);

    // A well-aged replay (seq 100 when cursor is at 501) is also stale.
    auto replay = jb.push(frame(100));
    ASSERT_TRUE(replay.is_err());
    EXPECT_EQ(replay.error(), JitterError::StalePacket);
}

TEST(JitterBuffer, OverflowDropsOldest) {
    JitterBuffer jb;
    ASSERT_TRUE(jb.push(frame(0, {0x00})).is_ok()); // arms cursor at 0
    ASSERT_TRUE(jb.push(frame(1, {0x01})).is_ok());
    ASSERT_TRUE(jb.push(frame(2, {0x02})).is_ok());
    // 10 is way ahead — overflow. Drop-oldest: cursor jumps to 10 - 2 = 8.
    ASSERT_TRUE(jb.push(frame(10, {0x0A})).is_ok());

    // Seq 0, 1, 2 are now behind the new cursor and must be gone.
    auto a = jb.pop(); ASSERT_TRUE(a.is_ok());
    EXPECT_EQ(a.value().sequence, 8);
    EXPECT_EQ(a.value().kind, PopKind::NeedsPLC);

    auto b = jb.pop(); ASSERT_TRUE(b.is_ok());
    EXPECT_EQ(b.value().sequence, 9);
    EXPECT_EQ(b.value().kind, PopKind::NeedsPLC);

    auto c = jb.pop(); ASSERT_TRUE(c.is_ok());
    EXPECT_EQ(c.value().sequence, 10);
    EXPECT_EQ(c.value().kind, PopKind::HasFrame);
    EXPECT_EQ(c.value().opus_payload[0], 0x0A);
}

TEST(JitterBuffer, ResetClearsState) {
    JitterBuffer jb;
    ASSERT_TRUE(jb.push(frame(77)).is_ok());
    EXPECT_TRUE(jb.armed());
    jb.reset();
    EXPECT_FALSE(jb.armed());
    EXPECT_EQ(jb.depth(), 0u);
}
