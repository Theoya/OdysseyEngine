# Proximity Voice Chat — Implementation Authorization

- Date: 2026-04-20
- Topic tag: physics=false
- Tally: approve=12, reject=0, abstain=5 → **100%** (threshold 80%)
- Outcome: **RATIFIED**

## Proposal

Add proximity voice chat to OdysseyEngine as a new subsystem at `src/audio/voice/` with Opus as a new vcpkg dependency, WASAPI capture (48kHz mono / 20ms frames), server-relayed transport requiring a `PROTOCOL_VERSION` bump (1→2) in `src/net/protocol.h`, per-tick interest filtering on a new `voice_range` entity stat, and a new public API `VoiceBus::preview_for_listener` backed by inverse-amplitude distance attenuation (d_min=1m, d_max=25m), equal-power panning, and distance-driven LPF. MusicDirector gains a sidechain-driven biquad peaking EQ carve at 3kHz / Q=1 / -4dB on the music bus (Marty KB §11), stacked with a -6dB whole-bus duck and a -3dB SFX duck when voice is active. Jitter buffer is 3 slots with Opus PLC. Design docs: `docs/design/proximity_chat_netcode.md`, `docs/design/proximity_chat_audio.md`.

## Votes

| Agent | Vote | Weight | Key point |
|---|---|---|---|
| game-ai-engineer | abstain | 2 | Voice is netcode+audio, not a Nadir perception signal — out of domain unless enemies later hear chat. |
| game-asset-engineer | approve | 2 | New `voice_range` stat must flow through `<stats>` XSD attrs so prefabs round-trip and agents can author declaratively. |
| game-engine-architect | approve | 2 | Matches engine grain: pure DSP, I/O at WASAPI/UDP boundary. Stacked ducks risk over-carving — make EQ carve and SFX duck Marty-tunable trims. |
| lighting-mood-architect | abstain | 3 | No visual mood surface touched. Route through them if voice ever drives a visible tell. |
| marty-odonnell-composer | approve | 4 | Carve + duck design honors voice as a soloist; demands composed attack/release envelopes so the mix doesn't pump. |
| netcode-engineer | approve | 2 | Server-relay + v1→v2 bump + interest filter reuse correct. Server-side speaker_entity_id rebind is the anti-spoof keystone. |
| vibe-story-guardian | approve | 2 | PTT must be default; voice must duck to -∞ during scripted cues; no chat-app chrome over heads. |

## Conditions adopted

- **asset — schema round-trip:** `voice_range` added to `schemas/prefab.xsd` and `schemas/scene.xsd` as optional `<stats>` attribute with documented default (25m = d_max). Loader omits when default; success+failure tests for missing/malformed/negative/NaN.
- **architect — purity:** `VoiceBus::preview_for_listener` is a pure function over `(frame, listener_pose, source_pose, params) → Result<MixedFrame,E>`. WASAPI submit and socket send live in thin wrappers under `src/audio/voice/io/`.
- **architect — derivation comments:** every DSP block (inverse-amplitude attenuation, equal-power pan, distance-LPF cutoff curve, RBJ biquad EQ coefficients, jitter buffer math) carries a derivation comment citing its source (RFC 6716, Zölzer DSP, Marty KB §11).
- **architect — Opus wrapping:** Opus used only behind an internal `odyssey::audio::voice::Codec` interface. A line-by-line explainer doc (`docs/internals/opus_explainer.md`) covers encode/decode/PLC calls we actually use. Council re-vote required before broadening the Opus surface.
- **architect — test surface:** success + failure unit tests per `Result<T,E>` entry (encode/decode/PLC, jitter underrun/overrun, out-of-range listener, invalid sample rate, NaN EQ coefficients).
- **architect — protocol compat:** `PROTOCOL_VERSION` bump paired with compatibility-rejection test (v1 client vs v2 server returns clean error, not UB).
- **marty — tunable sidechain:** EQ-carve gain/Q/freq exposed as engine.xml tunables for per-scene adjustment; different cues (dense choir vs sparse piano) need different carve depth.
- **marty — composed envelope:** ducking attack 30-50ms, release 200-400ms (not instantaneous). Derivation documented in `proximity_chat_audio.md`.
- **marty — full restore:** when no voice active for >2s, music bus restores to 0 dB reference. Pure function returning `target_gain(time_since_last_voice)`.
- **netcode — Opus explainer:** `docs/internals/opus_explainer.md` lands BEFORE merging the Opus vcpkg entry. Covers VOIP mode, CBR, complexity 5, PLC path.
- **netcode — protocol-diff gate:** `/protocol-diff` output attached to this decision record before the wire change merges.
- **netcode — test matrix:** VOICE_FRAME serialize/deserialize (happy, truncated, bad protocol_id, bad version, oversize); jitter buffer (in-order, out-of-order, duplicate, gap→PLC, END_OF_TALK flush); `preview_for_listener` (in range, out of range, at d_min, at d_max, hysteresis in/out); u16 `voice_sequence` wraparound; replay-attack stale-sequence drop.
- **netcode — anti-cheat hard-clamp:** server clamps ingress `voice_range` to 50m config max; compromised client cannot broadcast map-wide.
- **netcode — bandwidth accounting:** design doc §8 documents expected per-listener downstream at N concurrent speakers; code/test asserts no single listener exceeds existing per-connection budget.
- **vibe — PTT default:** default input gate = push-to-talk. Open-mic is opt-in per-session, never default.
- **vibe — score-priority bypass:** during a MusicDirector scripted cue or narrative beat, voice ducks to -∞ or bypasses (not merely -6 dB).
- **vibe — diegetic UI:** no floating chat-app speaker icons over heads. If shown, route through EVA HUD aesthetic as in-world telemetry.

## Additions adopted

- **architect:** frame-budget assertion — voice mix path <0.5ms on RTX 3080 for 32 concurrent sources; test fails if exceeded.
- **architect:** VoiceBus debug overlay hook in `src/debug/overlay` showing active sources, jitter depth, duck state — parallel to Nadir inspectability.
- **architect:** hysteresis envelope on sidechain duck (attack ~10ms, release ~200ms) so short utterances don't pump the score.
- **asset:** expose `voice_range` via existing MCP/CLI prefab-edit surface; agents author declaratively.
- **asset:** hot-reload path for `voice_range` on live entities.
- **marty:** `music_voice_priority` per music state — boss/combat cues optionally resist ducking more than ambient cues.
- **marty:** spatialized voice shares the 3D audio bus posture of diegetic SFX (distant teammate feels like it emanates from the world, not headset).
- **netcode:** silence suppression — frames not relayed when PTT released or VAD below threshold.
- **netcode:** per-speaker `voice_sequence` nonce in VOICE_FRAME header (future crypto drop-in).
- **netcode:** net-stats-dump fields for voice — in/out frames/s per listener, PLC invocation rate, jitter underrun count.
- **vibe:** scene-authored "quiet zones" — regions where voice is attenuated or muted entirely.
- **vibe:** far-distance LPF should go past gentle roll-off — voice at d_max should feel *failing*, not merely quiet (garbled, lossy, uncertain).

## Dissent recorded

None. Two abstentions (game-ai-engineer, lighting-mood-architect) on out-of-domain grounds; no rejects.

## Protocol diff (v1 → v2)

```
PROTOCOL_VERSION:  1 → 2   (src/net/protocol.h:18)

PacketType enum — additions only (no reorder, no removal):
  + VOICE_FRAME   = 40
  + VOICE_CONTROL = 41   (reserved for future PTT / mute-sync messages)

New struct (after PacketHeader when type == VOICE_FRAME):
  struct VoiceSubHeader {                       // 8 bytes, static_assert
      uint32_t speaker_entity_id;  // +0  REWRITTEN by server on relay
      uint16_t sequence;           // +4  per-speaker u16, wraps
      uint8_t  frame_ms;           // +6  20 today, 10/40/60 reserved
      uint8_t  flags;              // +7  bit0 VAD_ACTIVE
                                   //     bit1 PTT_HELD
                                   //     bit2 FEC_PRESENT (reserved)
                                   //     bit3 END_OF_TALK
                                   //     bit4-7 MUST be 0  (RESERVED_MASK)
  };

Wire layout for VOICE_FRAME packet:
  [IP 20][UDP 8][PacketHeader 16][VoiceSubHeader 8][opus_payload N]
  Typical: 112 B total. Hard cap MAX_PACKET_SIZE = 1200 B, never fragmented.

No existing packet type changes layout. ConnectPayload.version still
carries PROTOCOL_VERSION, so v1 clients connecting to v2 servers are
cleanly rejected by version mismatch (test: ProtocolVersionCompat).

Security invariants (test: VoiceRelay):
  - Server rewrites speaker_entity_id on every ingress (anti-spoof).
  - voice_range clamped to 50 m max on ingress.
  - Server never relays a voice frame back to its speaker.
  - END_OF_TALK frames are not relayed.
  - bits 4-7 of flags non-zero → ReservedFlagsSet error on deserialize.
```

## Follow-up triggers

- **game-ai-engineer:** if voice becomes an AI perception signal (enemies hearing chat), return to council as a Nadir-side change.
- **lighting-mood-architect:** if voice activity drives a visual indicator (rim glow on speaker, HUD tell), route through them before shipping.
- **marty / vibe:** composed envelope and PTT-default are load-bearing — any relaxation requires re-vote.
