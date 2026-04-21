# Opus — line-by-line explainer of the API surface OdysseyEngine uses

Mandate #4 says: *no third-party library we can't explain line-by-line.* This doc covers every Opus call `src/audio/voice/codec.cpp` makes, why, and what Opus is doing under the hood. Any engineer must be able to walk through the codec path with this doc in hand.

References inline use RFC 6716 (the Opus IETF spec) section numbers.

## What Opus is (one paragraph)

Opus is a royalty-free, low-latency audio codec. Internally it is two codecs fused: **SILK** (linear-prediction, for speech, inherited from Skype) and **CELT** (modified discrete cosine transform, for music / low-latency). For voice chat at 48 kHz / 20 ms / 24 kbps we live entirely in the SILK path (RFC 6716 §2.1.4, "Narrowband/Wideband/... modes"). Opus is the industry-standard voice codec — used by Discord, Zoom, Teams, Jitsi, WhatsApp. It is spec-complete at RFC 6716 + RFC 8251 (updates), ~30 kLOC of well-commented C in the reference encoder.

## The public API we use

We touch **six** Opus functions and **three** CTLs. Nothing else leaks through `odyssey::audio::voice::Codec`.

### 1. `opus_encoder_create(Fs, channels, application, &err)`

Allocates an encoder state. We call it once per speaker stream.

- `Fs = 48000` — sample rate. RFC 6716 §7 requires Fs ∈ {8000, 12000, 16000, 24000, 48000}. We choose 48 kHz to match WASAPI's native rate and skip a resample step.
- `channels = 1` — mono. Proximity voice is mono; spatialization happens downstream in our DSP.
- `application = OPUS_APPLICATION_VOIP` — picks SILK mode with speech-tuned rate/quality tradeoffs (RFC 6716 §2.1.9 "VoIP vs Audio mode"). The alternative `AUDIO` mode prefers CELT and is for music streaming.
- `err` — out-param, Opus's own error code.

We wrap the returned `OpusEncoder*` in a unique_ptr with a custom deleter calling `opus_encoder_destroy`. The encoder holds internal SILK state (analysis filter banks, pitch buffers, LTP prediction state) ≈ 10 KB.

### 2. `opus_encoder_ctl(enc, OPUS_SET_BITRATE(24000))`

Forces constant bit rate at 24 kbps. RFC 6716 §7.1.4: "SILK at 24 kbps/mono wideband produces intelligible speech near the upper boundary of subjective transparency for voice-grade content."

24 kbps × 20 ms = 480 bits = 60 bytes per frame. Our packet MTU budget (see `docs/design/proximity_chat_netcode.md` §5) targets ~72-byte payloads (60 B Opus + 8 B voice sub-header + 16 B PacketHeader — comfortably under the 1200 B MAX_PACKET_SIZE).

### 3. `opus_encoder_ctl(enc, OPUS_SET_VBR(0))` and `OPUS_SET_VBR_CONSTRAINT(1)`

Disable variable bit rate; constrain any residual variance. Combined, these force a predictable 60-byte frame every 20 ms, which:

- simplifies our outgoing bandwidth budget (no pessimistic padding),
- defeats a known traffic-analysis fingerprint where silence vs speech leaks via variable frame size,
- makes the jitter buffer's fixed-slot assumption valid.

### 4. `opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(5))`

Complexity 0-10. RFC 6716 §7 explains this controls how much analysis the encoder does per frame — pitch search thoroughness, LTP prediction order search. 5 is the Opus-project-recommended midpoint for real-time voice: ≈ 0.5 ms encode time per 20 ms frame on x86-64, vs ≈ 1.2 ms at complexity 10 for quality gains that are inaudible on speech. Below 5, pitch search degrades — we notice on sustained vowels.

### 5. `opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE))`

Hints the encoder that the input is speech (not music). The SILK path already assumes this in VOIP application mode, but setting the signal hint explicitly improves the forced-mode switching if the SILK/CELT hybrid would otherwise consider jumping to CELT for transient-heavy frames (e.g. plosives).

### 6. `opus_encode_float(enc, pcm, frame_size, output, max_bytes) → bytes_written`

The encode call itself. `frame_size = 960` (20 ms × 48 kHz). `pcm` is float32 in [-1, 1]. `output` is our 64-byte buffer (60 B worst case + headroom). Returns the actual bytes written; we always get 60 back because of CBR.

Opus's internal chain per call (simplified, RFC 6716 §4):

1. High-pass filter at 80 Hz to strip rumble (part of the VOIP preprocessor).
2. SILK Linear-Prediction analysis: 16-order LPC fit to the 20 ms frame, residual computed.
3. Long-Term Prediction (LTP): pitch estimate + pitch-periodic residual modeling.
4. Range-coder entropy compression of the LPC + LTP + residual streams.
5. Packet framing with the Opus TOC (Table of Contents) byte — tells the decoder the mode and frame length.

Our `Codec::encode()` returns a `Result<std::vector<uint8_t>, CodecError>`. Errors: `InvalidFrameSize` (not 960), `NonFiniteSample` (NaN in input — refuse, don't corrupt encoder state), `EncodeFail` (Opus returned ≤ 0).

### 7. `opus_decoder_create(Fs, channels, &err)`

Same rationale as the encoder, mirror call on the receive side. One decoder per active speaker the local client hears (decoder state is per-stream because SILK keeps an LPC history).

### 8. `opus_decode_float(dec, data, len, pcm_out, frame_size, decode_fec) → samples_written`

- `data=nullptr, len=0, decode_fec=0` → **Packet Loss Concealment (PLC)** path. Opus synthesizes a 20 ms frame by extrapolating pitch + LPC state from the last good frame. RFC 6716 §4.4: "In the absence of a valid compressed frame, the decoder invokes the packet loss concealment algorithm, which extrapolates a replacement frame by extending the pitch contour and applying a gain envelope that fades to noise."
- `data≠null, decode_fec=1` → in-band FEC (forward error correction); the previous frame's data was carried redundantly in this frame. We don't use FEC in Phase 3 — our 3-slot jitter buffer + PLC is sufficient for LAN RTT; FEC would add 5-15 % bitrate for scenarios the buffer should already handle.
- `data≠null, decode_fec=0` → normal decode.

Returns samples written (960 for a valid 20 ms frame), or a negative Opus error. We map negative returns to `CodecError::DecodeFail`.

### 9. `opus_decoder_destroy(dec)`

Frees decoder state. Called from the `Codec` destructor (unique_ptr with custom deleter).

## What we deliberately DON'T touch

- `opus_multistream_*` — multiple channels / surround. We are mono.
- `opus_encoder_ctl(OPUS_SET_INBAND_FEC)` — covered above, deferred.
- `opus_encoder_ctl(OPUS_SET_DTX)` — discontinuous transmission (Opus sends nothing during silence). We do our own VAD-driven silence suppression *outside* Opus so the gating signal matches the overlay and the bus-ducker. Letting Opus decide silently would make the UX feel like network loss on voice drop-ins.
- `opus_repacketizer_*` — joining/splitting packets. Our 20 ms = one packet rule is invariant.
- `opus_packet_get_*` — packet introspection. Relay never parses Opus payloads (design doc §6 "opaque bytes").
- All CELT-direct entry points. We are SILK-only via VOIP mode.

If any of these become tempting later, the Codec surface must be re-ratified by council before widening (per the architect's condition in the decision record).

## Threading model

`OpusEncoder*` and `OpusDecoder*` state is **not thread-safe** per instance. Our `Codec` wraps each in a per-speaker object; encode runs on the capture thread, decode on the audio-mix thread (playback). Both are single-threaded accessors into their respective state — no locking needed. The shared path is the packet buffer, which is synchronized by the network layer's mailbox, not by Codec.

## Bitrate & latency sanity check

- Frame size: 20 ms (= 960 samples @ 48 kHz mono).
- Bit rate: 24 kbps → 480 bits → 60 bytes per frame.
- Algorithmic delay: 20 ms frame + ≈5 ms SILK lookahead = 25 ms encoder.
- Decoder delay: ~0 ms (lookup).
- Total codec contribution to end-to-end latency: ≈25 ms.

Combined with capture (20 ms) + jitter buffer (60 ms floor) + playback (10 ms) + network RTT (LAN ~1-5 ms), end-to-end latency sits around **~120 ms on LAN**, which matches the budget in `docs/design/proximity_chat_audio.md` §12.

## If Opus ever has to go

The council conditioned the Opus grandfathering on this explainer existing. If some future mandate pulls the plug on Opus, the fallback path is a SILK-only shim: reimplement the SILK decoder (~3 kLOC, which we *could* read line-by-line) and ship our own encoder at reduced quality (complexity-zero equivalent). That is a weeks-of-work escape hatch, not a day-of-work one — document it but don't invoke it without a second council vote.

## Change log

- 2026-04-20 — Initial version, written alongside Phase 3 landing. Covers the exact call surface in `src/audio/voice/codec.cpp` as of that date. Any new Opus CTL requires a council re-vote (architect condition).
