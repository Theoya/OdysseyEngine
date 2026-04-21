// Codec tests — validate(), encode/decode round-trip, PLC path, error modes.
// Per Mandate #2: success + failure for every Result<T,E> entry.

#include <gtest/gtest.h>

#include "audio/voice/codec.h"

#include <cmath>
#include <vector>

using namespace odyssey;
using namespace odyssey::audio::voice;

namespace {

// Generate a short sine tone at a given freq as mono f32 samples in [-1, 1].
std::vector<float> sine(int sample_rate, int samples, float hz, float amp = 0.5f) {
    std::vector<float> pcm(samples);
    const float w = 2.0f * 3.1415926535f * hz / static_cast<float>(sample_rate);
    for (int i = 0; i < samples; ++i) {
        pcm[i] = amp * std::sin(w * static_cast<float>(i));
    }
    return pcm;
}

CodecConfig default_cfg() {
    CodecConfig c;
    c.sample_rate = 48000;
    c.channels = 1;
    c.frame_ms = 20;
    c.bitrate_bps = 24000;
    c.complexity = 5;
    c.use_cbr = true;
    return c;
}

} // namespace

// ─── validate() error cases ──────────────────────────────────────────────────

TEST(CodecValidate, AcceptsDefault) {
    auto r = validate(default_cfg());
    ASSERT_TRUE(r.is_ok());
}

TEST(CodecValidate, RejectsBadSampleRate) {
    CodecConfig c = default_cfg();
    c.sample_rate = 44100; // not in {8,12,16,24,48} kHz
    auto r = validate(c);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), CodecError::InvalidSampleRate);
}

TEST(CodecValidate, RejectsBadChannels) {
    CodecConfig c = default_cfg();
    c.channels = 3;
    auto r = validate(c);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), CodecError::InvalidChannels);
}

TEST(CodecValidate, RejectsBadFrameMs) {
    CodecConfig c = default_cfg();
    c.frame_ms = 17; // not in {5, 10, 20, 40, 60}
    auto r = validate(c);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), CodecError::InvalidFrameMs);
}

TEST(CodecValidate, RejectsComplexityOutOfRange) {
    CodecConfig c = default_cfg();
    c.complexity = 99;
    auto r = validate(c);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), CodecError::InvalidComplexity);

    c.complexity = -1;
    auto r2 = validate(c);
    ASSERT_TRUE(r2.is_err());
    EXPECT_EQ(r2.error(), CodecError::InvalidComplexity);
}

TEST(CodecValidate, RejectsBadBitrate) {
    CodecConfig c = default_cfg();
    c.bitrate_bps = 100; // well below Opus floor
    auto r = validate(c);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), CodecError::InvalidBitrate);
}

// ─── samples_per_frame (pure math) ───────────────────────────────────────────

TEST(CodecSamplesPerFrame, 48k20ms) {
    CodecConfig c = default_cfg();
    // Derivation: 48000 × 0.020 = 960 samples/channel. RFC 6716 §2.1.4.
    EXPECT_EQ(samples_per_frame(c), 960);
}

// ─── encode/decode round trip ────────────────────────────────────────────────

TEST(CodecEncode, RejectsPcmSizeMismatch) {
    CodecConfig c = default_cfg();
    auto enc = make_encoder(c);
    ASSERT_TRUE(enc.is_ok()) << to_string(enc.error());
    auto handle = std::move(enc).value();

    // Wrong size PCM.
    std::vector<float> bad(100, 0.0f);
    auto r = encode(handle, c, bad.data(), bad.size());
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), CodecError::PcmSizeMismatch);
}

TEST(CodecRoundTrip, EncodeDecodeSine) {
    CodecConfig c = default_cfg();

    auto enc = make_encoder(c);
    ASSERT_TRUE(enc.is_ok()) << to_string(enc.error());
    auto dec = make_decoder(c);
    ASSERT_TRUE(dec.is_ok()) << to_string(dec.error());
    auto encoder = std::move(enc).value();
    auto decoder = std::move(dec).value();

    auto pcm = sine(c.sample_rate, samples_per_frame(c), 440.0f);

    auto encoded = encode(encoder, c, pcm.data(), pcm.size());
    ASSERT_TRUE(encoded.is_ok()) << to_string(encoded.error());
    // At 24 kbps CBR × 20 ms, expect ≈ 60 bytes. Allow a wide window because
    // startup frames from the encoder can produce a small pre-roll.
    EXPECT_GT(encoded.value().size(), 10u);
    EXPECT_LT(encoded.value().size(), 300u);

    auto decoded = decode(decoder, c,
                          encoded.value().data(),
                          encoded.value().size());
    ASSERT_TRUE(decoded.is_ok()) << to_string(decoded.error());
    EXPECT_EQ(decoded.value().size(), static_cast<size_t>(samples_per_frame(c)));
}

TEST(CodecPLC, NullPayloadInvokesConcealment) {
    CodecConfig c = default_cfg();
    auto dec = make_decoder(c);
    ASSERT_TRUE(dec.is_ok());
    auto decoder = std::move(dec).value();

    // PLC call: payload=null, len=0. Must succeed and return a full frame of
    // samples (the concealed audio).
    auto r = plc(decoder, c);
    ASSERT_TRUE(r.is_ok()) << to_string(r.error());
    EXPECT_EQ(r.value().size(), static_cast<size_t>(samples_per_frame(c)));
}

TEST(CodecPLC, ViaDecodeWithNull) {
    // Redundant of plc() but exercises the decode() overload directly, the
    // path actually taken by the jitter buffer consumer.
    CodecConfig c = default_cfg();
    auto dec = make_decoder(c);
    ASSERT_TRUE(dec.is_ok());
    auto decoder = std::move(dec).value();

    auto r = decode(decoder, c, nullptr, 0);
    ASSERT_TRUE(r.is_ok());
}

// ─── Factory error paths ─────────────────────────────────────────────────────

TEST(CodecFactory, MakeEncoderRejectsBadConfig) {
    CodecConfig c = default_cfg();
    c.sample_rate = 99999;
    auto r = make_encoder(c);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), CodecError::InvalidSampleRate);
}

TEST(CodecFactory, MakeDecoderRejectsBadConfig) {
    CodecConfig c = default_cfg();
    c.channels = 7;
    auto r = make_decoder(c);
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error(), CodecError::InvalidChannels);
}
