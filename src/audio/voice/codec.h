#pragma once
// odyssey::audio::voice::Codec — narrow, engine-owned surface around libopus.
//
// Council condition (architect): "Opus used only behind an internal
// `odyssey::audio::voice::Codec` interface. A line-by-line explainer doc
// (`docs/internals/opus_explainer.md`) covers encode/decode/PLC calls we
// actually use."
//
// This header intentionally does NOT include <opus/opus.h>. If a call site
// needs Opus-specific symbols, the correct answer is to extend this surface
// and re-open the council vote (per the decision record), not to punch
// through it.
//
// Engine grain:
//   - CodecConfig and encode/decode/plc operate on byte buffers + PCM spans
//     (pure data in, pure data out, no hidden globals).
//   - OpusEncoderHandle / OpusDecoderHandle are opaque pimpls that own the
//     libopus state — their lifetime IS the I/O boundary (Mandate #1:
//     "side effects isolated to thin I/O boundary wrappers").
//   - Every Result<T,E>-returning entry point has paired success + failure
//     tests (Mandate #2).
//
// RFC 6716 calls we wrap (full walkthrough in docs/internals/opus_explainer.md):
//   opus_encoder_create     — allocate state, select FB/SB/WB/MB/NB internally
//   opus_encoder_ctl        — set VOIP mode, CBR, complexity 5
//   opus_encode_float       — encode 20ms of f32 [-1,1] PCM → bytes
//   opus_decoder_create     — allocate decoder state
//   opus_decode_float       — decode bytes → f32 PCM (NULL input triggers PLC)
//   opus_decoder_ctl        — query decoder state (sample count etc.)

#include "core/result.h"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace odyssey::audio::voice {

enum class CodecError {
    InvalidSampleRate,   // not in {8000, 12000, 16000, 24000, 48000}
    InvalidChannels,     // not 1 or 2
    InvalidFrameMs,      // not in {2.5, 5, 10, 20, 40, 60} × sample_rate/1000 check
    InvalidComplexity,   // not in [0, 10]
    InvalidBitrate,      // not in [6000, 510000]
    EncoderAllocFailed,  // libopus returned OPUS_ALLOC_FAIL / null
    DecoderAllocFailed,
    EncodeFailed,        // libopus encode returned a negative error code
    DecodeFailed,        // libopus decode returned a negative error code
    CtlFailed,           // opus_*_ctl returned non-OPUS_OK
    PcmSizeMismatch,     // input PCM span length != samples_per_frame * channels
    BufferTooSmall,      // encoded output cap exceeded
};

std::string to_string(CodecError e);

// Codec configuration. Defaults reflect the council-ratified choices:
// 48 kHz mono, 20 ms frames, 24 kbps CBR, complexity 5, VOIP application.
// See docs/design/proximity_chat_netcode.md §3.
struct CodecConfig {
    int32_t sample_rate   = 48000; // Hz; Opus spec sample rates only
    int32_t channels      = 1;     // mono speech
    int32_t frame_ms      = 20;    // must yield integer samples at sample_rate
    int32_t bitrate_bps   = 24000; // CBR target
    int32_t complexity    = 5;     // 0..10; trades CPU for quality
    bool    use_cbr       = true;  // OPUS_SET_VBR(0)
    // OPUS_APPLICATION_VOIP is fixed internally; not a user knob.
};

// Opaque pimpl around OpusEncoder*. Destructor calls opus_encoder_destroy.
class OpusEncoderHandle {
public:
    OpusEncoderHandle();
    ~OpusEncoderHandle();
    OpusEncoderHandle(const OpusEncoderHandle&) = delete;
    OpusEncoderHandle& operator=(const OpusEncoderHandle&) = delete;
    OpusEncoderHandle(OpusEncoderHandle&&) noexcept;
    OpusEncoderHandle& operator=(OpusEncoderHandle&&) noexcept;

    struct Impl;
    Impl* impl() const { return impl_.get(); }

private:
    std::unique_ptr<Impl> impl_;
    friend Result<OpusEncoderHandle, CodecError>
    make_encoder(const CodecConfig& cfg);
};

class OpusDecoderHandle {
public:
    OpusDecoderHandle();
    ~OpusDecoderHandle();
    OpusDecoderHandle(const OpusDecoderHandle&) = delete;
    OpusDecoderHandle& operator=(const OpusDecoderHandle&) = delete;
    OpusDecoderHandle(OpusDecoderHandle&&) noexcept;
    OpusDecoderHandle& operator=(OpusDecoderHandle&&) noexcept;

    struct Impl;
    Impl* impl() const { return impl_.get(); }

private:
    std::unique_ptr<Impl> impl_;
    friend Result<OpusDecoderHandle, CodecError>
    make_decoder(const CodecConfig& cfg);
};

// ─── Pure-ish config math (derivation-commented, first-principles) ───────────
// samples_per_frame = sample_rate × frame_ms / 1000.
// For 48 kHz × 20 ms = 960 samples per channel (RFC 6716 §2.1.4).
int32_t samples_per_frame(const CodecConfig& cfg);

// Validate cfg without touching libopus. Allows callers (and tests) to
// catch bad config before the alloc path. Exposed as a free function so
// callers can probe without committing to an encoder alloc.
Result<bool, CodecError> validate(const CodecConfig& cfg);

// ─── Encoder/decoder factories (I/O boundary — allocate libopus state) ───────
Result<OpusEncoderHandle, CodecError> make_encoder(const CodecConfig& cfg);
Result<OpusDecoderHandle, CodecError> make_decoder(const CodecConfig& cfg);

// Encode one frame of interleaved float PCM in [-1, 1]. pcm.size() must equal
// samples_per_frame(cfg) * cfg.channels. Returns the Opus byte payload; typical
// 60 B at 24 kbps × 20 ms. The internal output cap is MAX_PACKET_SIZE - header
// size so we never produce a frame that can't ride one UDP datagram.
Result<std::vector<uint8_t>, CodecError>
encode(OpusEncoderHandle& enc,
       const CodecConfig& cfg,
       const float* pcm,
       size_t pcm_len);

// Decode one frame. `payload` may be empty/null to invoke packet-loss
// concealment (PLC) per RFC 6716 §4.4 — opus_decode_float(dec, NULL, 0, pcm,
// samples_per_frame, 0) synthesizes a plausible frame from decoder memory.
// Output pcm vector is sized to samples_per_frame(cfg) * cfg.channels.
Result<std::vector<float>, CodecError>
decode(OpusDecoderHandle& dec,
       const CodecConfig& cfg,
       const uint8_t* payload,
       size_t payload_len);

// Convenience: force PLC. Equivalent to decode(..., nullptr, 0). Named so
// jitter buffer call sites read as "plc()" at the gap point, not "decode a
// null payload" which obscures intent.
Result<std::vector<float>, CodecError>
plc(OpusDecoderHandle& dec, const CodecConfig& cfg);

} // namespace odyssey::audio::voice
