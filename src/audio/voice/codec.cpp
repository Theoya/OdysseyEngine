#include "audio/voice/codec.h"
#include "net/protocol.h" // MAX_PACKET_SIZE

#include <opus/opus.h>

#include <cstring>
#include <utility>

namespace odyssey::audio::voice {

// ─── Impl structs (hold the raw libopus state) ────────────────────────────────

struct OpusEncoderHandle::Impl {
    OpusEncoder* enc = nullptr;
    ~Impl() {
        if (enc) {
            opus_encoder_destroy(enc);
            enc = nullptr;
        }
    }
};

struct OpusDecoderHandle::Impl {
    OpusDecoder* dec = nullptr;
    ~Impl() {
        if (dec) {
            opus_decoder_destroy(dec);
            dec = nullptr;
        }
    }
};

// Default ctors allocate an empty pimpl so moves don't UAF against nullptr.
OpusEncoderHandle::OpusEncoderHandle() : impl_(std::make_unique<Impl>()) {}
OpusEncoderHandle::~OpusEncoderHandle() = default;
OpusEncoderHandle::OpusEncoderHandle(OpusEncoderHandle&&) noexcept = default;
OpusEncoderHandle& OpusEncoderHandle::operator=(OpusEncoderHandle&&) noexcept = default;

OpusDecoderHandle::OpusDecoderHandle() : impl_(std::make_unique<Impl>()) {}
OpusDecoderHandle::~OpusDecoderHandle() = default;
OpusDecoderHandle::OpusDecoderHandle(OpusDecoderHandle&&) noexcept = default;
OpusDecoderHandle& OpusDecoderHandle::operator=(OpusDecoderHandle&&) noexcept = default;

// ─── Strings / pure helpers ───────────────────────────────────────────────────

std::string to_string(CodecError e) {
    switch (e) {
        case CodecError::InvalidSampleRate:  return "InvalidSampleRate";
        case CodecError::InvalidChannels:    return "InvalidChannels";
        case CodecError::InvalidFrameMs:     return "InvalidFrameMs";
        case CodecError::InvalidComplexity:  return "InvalidComplexity";
        case CodecError::InvalidBitrate:     return "InvalidBitrate";
        case CodecError::EncoderAllocFailed: return "EncoderAllocFailed";
        case CodecError::DecoderAllocFailed: return "DecoderAllocFailed";
        case CodecError::EncodeFailed:       return "EncodeFailed";
        case CodecError::DecodeFailed:       return "DecodeFailed";
        case CodecError::CtlFailed:          return "CtlFailed";
        case CodecError::PcmSizeMismatch:    return "PcmSizeMismatch";
        case CodecError::BufferTooSmall:     return "BufferTooSmall";
    }
    return "Unknown";
}

// samples_per_frame = Fs × Tms / 1000.
// Derivation: a 20 ms frame at 48 kHz is 48000 × 0.020 = 960 samples / channel.
// RFC 6716 §2.1.4 enumerates exactly {2.5, 5, 10, 20, 40, 60} ms frame sizes.
int32_t samples_per_frame(const CodecConfig& cfg) {
    return (cfg.sample_rate * cfg.frame_ms) / 1000;
}

Result<bool, CodecError> validate(const CodecConfig& cfg) {
    using R = Result<bool, CodecError>;

    // Opus supports exactly these internal sampling rates — RFC 6716 §2.1.1.
    switch (cfg.sample_rate) {
        case 8000: case 12000: case 16000: case 24000: case 48000: break;
        default: return R::err(CodecError::InvalidSampleRate);
    }
    if (cfg.channels != 1 && cfg.channels != 2) {
        return R::err(CodecError::InvalidChannels);
    }
    // Accept frame sizes that yield an integer sample count and are in the
    // legal set. 2.5 ms isn't representable as int ms, so not supported.
    switch (cfg.frame_ms) {
        case 5: case 10: case 20: case 40: case 60: break;
        default: return R::err(CodecError::InvalidFrameMs);
    }
    if (cfg.complexity < 0 || cfg.complexity > 10) {
        return R::err(CodecError::InvalidComplexity);
    }
    // Opus accepts 500..512000, but OPUS_AUTO is magic. We clamp to the
    // documented speech-usable range.
    if (cfg.bitrate_bps < 6000 || cfg.bitrate_bps > 510000) {
        return R::err(CodecError::InvalidBitrate);
    }
    return R::ok(true);
}

// ─── Factories ────────────────────────────────────────────────────────────────

Result<OpusEncoderHandle, CodecError> make_encoder(const CodecConfig& cfg) {
    using R = Result<OpusEncoderHandle, CodecError>;

    auto v = validate(cfg);
    if (v.is_err()) return R::err(v.error());

    int err = OPUS_OK;
    OpusEncoder* enc = opus_encoder_create(
        cfg.sample_rate,
        cfg.channels,
        OPUS_APPLICATION_VOIP, // council-ratified: VOIP mode
        &err);
    if (!enc || err != OPUS_OK) {
        if (enc) opus_encoder_destroy(enc);
        return R::err(CodecError::EncoderAllocFailed);
    }

    // Apply council-ratified tunables. Each opus_encoder_ctl call is walked
    // through line-by-line in docs/internals/opus_explainer.md.
    if (opus_encoder_ctl(enc, OPUS_SET_BITRATE(cfg.bitrate_bps)) != OPUS_OK) {
        opus_encoder_destroy(enc);
        return R::err(CodecError::CtlFailed);
    }
    if (opus_encoder_ctl(enc, OPUS_SET_VBR(cfg.use_cbr ? 0 : 1)) != OPUS_OK) {
        opus_encoder_destroy(enc);
        return R::err(CodecError::CtlFailed);
    }
    if (opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(cfg.complexity)) != OPUS_OK) {
        opus_encoder_destroy(enc);
        return R::err(CodecError::CtlFailed);
    }
    // DTX off: the council explicitly picked PTT/VAD-gated silence suppression
    // over in-codec DTX because on-wire we prefer "silence = no packet".
    if (opus_encoder_ctl(enc, OPUS_SET_DTX(0)) != OPUS_OK) {
        opus_encoder_destroy(enc);
        return R::err(CodecError::CtlFailed);
    }
    // Signal type = voice; matches VOIP application and helps SILK bias.
    if (opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)) != OPUS_OK) {
        opus_encoder_destroy(enc);
        return R::err(CodecError::CtlFailed);
    }

    OpusEncoderHandle h;
    h.impl_->enc = enc;
    return R::ok(std::move(h));
}

Result<OpusDecoderHandle, CodecError> make_decoder(const CodecConfig& cfg) {
    using R = Result<OpusDecoderHandle, CodecError>;

    auto v = validate(cfg);
    if (v.is_err()) return R::err(v.error());

    int err = OPUS_OK;
    OpusDecoder* dec = opus_decoder_create(cfg.sample_rate, cfg.channels, &err);
    if (!dec || err != OPUS_OK) {
        if (dec) opus_decoder_destroy(dec);
        return R::err(CodecError::DecoderAllocFailed);
    }

    OpusDecoderHandle h;
    h.impl_->dec = dec;
    return R::ok(std::move(h));
}

// ─── Encode ───────────────────────────────────────────────────────────────────

Result<std::vector<uint8_t>, CodecError>
encode(OpusEncoderHandle& enc,
       const CodecConfig& cfg,
       const float* pcm,
       size_t pcm_len)
{
    using R = Result<std::vector<uint8_t>, CodecError>;

    if (!enc.impl() || !enc.impl()->enc) {
        return R::err(CodecError::EncoderAllocFailed);
    }
    const int32_t spf = samples_per_frame(cfg);
    const size_t required = static_cast<size_t>(spf) * static_cast<size_t>(cfg.channels);
    if (pcm_len != required || pcm == nullptr) {
        return R::err(CodecError::PcmSizeMismatch);
    }

    // Output cap: never produce a frame that can't ride one MTU-safe UDP
    // packet after the 16 B PacketHeader + 8 B voice sub-header. Going
    // smaller saves memory; going larger risks fragmentation.
    constexpr size_t kOverhead = 16 + 8;
    const size_t max_out = odyssey::net::MAX_PACKET_SIZE - kOverhead;

    std::vector<uint8_t> out(max_out);
    int bytes = opus_encode_float(
        enc.impl()->enc,
        pcm,
        spf,
        out.data(),
        static_cast<opus_int32>(out.size()));
    if (bytes < 0) {
        return R::err(CodecError::EncodeFailed);
    }
    if (static_cast<size_t>(bytes) > max_out) {
        return R::err(CodecError::BufferTooSmall);
    }
    out.resize(static_cast<size_t>(bytes));
    return R::ok(std::move(out));
}

// ─── Decode ───────────────────────────────────────────────────────────────────

Result<std::vector<float>, CodecError>
decode(OpusDecoderHandle& dec,
       const CodecConfig& cfg,
       const uint8_t* payload,
       size_t payload_len)
{
    using R = Result<std::vector<float>, CodecError>;

    if (!dec.impl() || !dec.impl()->dec) {
        return R::err(CodecError::DecoderAllocFailed);
    }
    const int32_t spf = samples_per_frame(cfg);
    std::vector<float> pcm(static_cast<size_t>(spf) * static_cast<size_t>(cfg.channels));

    // Per RFC 6716 §4.4 + Opus API: passing NULL/0 invokes PLC. libopus is
    // strict — a non-null pointer with length 0 is NOT the same as PLC; the
    // opus docs explicitly say `data == NULL` means PLC, so we normalize.
    const unsigned char* data_ptr =
        (payload_len == 0) ? nullptr
                           : reinterpret_cast<const unsigned char*>(payload);
    const opus_int32 data_len = (payload_len == 0) ? 0 : static_cast<opus_int32>(payload_len);

    // decode_fec = 0 — we don't use in-band FEC in v2.0 (FEC_PRESENT flag
    // is reserved). If we enable it, this becomes 1 when concealing a lost
    // frame that the NEXT packet is known to contain FEC for.
    int samples = opus_decode_float(
        dec.impl()->dec,
        data_ptr,
        data_len,
        pcm.data(),
        spf,
        /*decode_fec=*/0);
    if (samples < 0) {
        return R::err(CodecError::DecodeFailed);
    }
    // libopus may return fewer samples than requested in edge cases; pad
    // with zeros if so (safer for WASAPI than leaving garbage).
    if (static_cast<size_t>(samples) * static_cast<size_t>(cfg.channels) < pcm.size()) {
        std::memset(pcm.data() + samples * cfg.channels, 0,
                    (pcm.size() - samples * cfg.channels) * sizeof(float));
    }
    return R::ok(std::move(pcm));
}

Result<std::vector<float>, CodecError>
plc(OpusDecoderHandle& dec, const CodecConfig& cfg) {
    return decode(dec, cfg, nullptr, 0);
}

} // namespace odyssey::audio::voice
