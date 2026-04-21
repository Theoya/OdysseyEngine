# Proximity Voice Chat — Audio Layer Design

**Owner:** marty-odonnell-composer (audio layer)
**Coordinates with:** netcode-engineer (wire protocol — see `docs/design/proximity_chat_netcode.md`)
**Scope:** capture → encoder handoff → decoded playback → 3D spatialization → bus routing → ducking.
**Out of scope:** packet framing, jitter-buffer network side, NAT, authentication, codec choice rationale.
**Platform:** Windows only. WASAPI. No middleware (per Mandate #4).

---

## 1. Signal chain (end to end)

```
  [ mic ] ── WASAPI capture ──▶ VAD gate ──▶ Opus encoder ──▶ [ net tx ]
                                                                  │
                                                             (netcode)
                                                                  │
  [ net rx ] ──▶ jitter buffer ──▶ Opus decoder ──▶ VoiceSource ──▶ VoiceBus
                                                         │              │
                                                  3D spatialize    mix + duck
                                                         │              │
                                                         └────▶ Mixer master ──▶ WASAPI render
```

Every DSP block in this chain is a **pure function** on PCM buffers. Only the two WASAPI endpoints and the encoder/decoder hold state; they are the thin I/O wrappers.

---

## 2. WASAPI capture

### 2.1 Device and format
- **Mode:** shared (co-operates with Discord, browser, OS). Exclusive mode is reserved for future low-latency render only.
- **Format request:** 48 kHz, mono, IEEE float32. Match the render clock so no SRC on the capture path.
- **Frame size:** 20 ms = 960 samples. Opus likes 20 ms; WASAPI will give us what fits the endpoint buffer, and we re-chunk to 960-sample frames in a ring buffer.

### 2.2 Activation sequence
1. `CoInitializeEx(nullptr, COINIT_MULTITHREADED)` once per audio thread.
2. `IMMDeviceEnumerator::GetDefaultAudioEndpoint(eCapture, eCommunications)` — the **communications** role, not `eConsole`. Windows routes Bluetooth headsets correctly this way and honors the user's mic-privacy settings.
3. `IAudioClient::Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, hnsBufferDuration=200 ms, 0, &fmt, nullptr)`. AUTOCONVERT + SRC let WASAPI reformat if the endpoint disagrees; we still prefer a native-48k mic.
4. `IAudioClient::SetEventHandle(hEvent)` — the capture thread waits on this event, not a polled sleep.
5. `IAudioCaptureClient::GetBuffer` → memcpy float32 samples into our ring buffer, `ReleaseBuffer`.

### 2.3 Device switching mid-session
Headsets plug/unplug. The rule:

- Register `IMMNotificationClient::OnDefaultDeviceChanged` for `eCapture / eCommunications`.
- On notification: mark `capture_dirty = true` (atomic). The audio thread, on its next event wake, tears down the current `IAudioClient`, re-enumerates, and re-initializes. Emit a `VoiceEvent::InputDeviceChanged` for UI.
- During the gap (typically 20–80 ms), we stop transmitting and send one `VOICE_SILENCE` packet so remote decoders flush. We do **not** buffer during the switch — stale audio from the old device would be confusing.

### 2.4 Endpoint error recovery
- `AUDCLNT_E_DEVICE_INVALIDATED` → same path as device switch.
- `AUDCLNT_E_BUFFER_TOO_LARGE` / starvation → log, skip frame, continue.

---

## 3. Voice activity detection (VAD)

Gate silence at the source so we don't spend bandwidth or CPU on a decoder that will render −60 dB noise.

### 3.1 First-principles RMS + zero-crossing gate

For a 20 ms frame of N = 960 samples `x[n]`:

```
        ┌─── 1  N-1        ┐
RMS =   │   ─── Σ  x[n]²   │ ^ ½
        └   N  n=0         ┘

            N-1
ZCR = (1/N) Σ   1 { sign(x[n]) ≠ sign(x[n-1]) }
            n=1
```

- **RMS** measures energy. Speech energy is typically −40 to −15 dBFS. Noise floor on a decent headset sits near −55 dBFS.
- **ZCR** measures high-frequency content. Pure tones and hum have low ZCR (few crossings per second). Voiced speech is ~0.05–0.15. Fricatives and noise are higher. The combination rejects both "silent room" (low RMS, low ZCR) and "fan hum" (moderate RMS, very low ZCR).

### 3.2 Hysteresis thresholds

Single thresholds chatter. Use two:

```
open_rms   = -38 dBFS   (gate opens above this)
close_rms  = -45 dBFS   (gate closes below this)
min_zcr    = 0.02       (reject DC/hum even if RMS passes)
hang_ms    = 200        (keep gate open for 200 ms after last active frame)
```

Hang-time prevents the gate from chopping the tails of words ("...hello— *click*"). 200 ms is one syllable's worth of release.

### 3.3 Pure interface
```
bool vad_is_active(span<const float> frame, VadState& state, const VadParams& p);
```
`VadState` holds `last_active_frame_idx` and the previous sample's sign for cross-frame ZCR continuity. Pure in/out; no WASAPI, no network.

---

## 4. Encoder handoff

Netcode doc selects **Opus** (libopus — needs its own council vote before merge; not listed yet in `vcpkg.json`).

### 4.1 Contract
```
encoder_input   : float32 PCM, mono, 48 kHz, 960 samples (20 ms), range [-1, +1] clipped
encoder_output  : opaque byte span, ≤ 4000 bytes per frame (we target ~32 kbps → ~80 bytes/frame)
```

- No DC block, no AGC, no noise suppression at this layer. Opus has internal VAD/DTX which is **disabled** — our VAD is canonical. We want deterministic gating we can visualize.
- If `vad_is_active == false` → skip encode, transmit `VOICE_SILENCE` (netcode handles).
- Sample range: we **do not** soft-clip. If the mic clips the engineer tuned it wrong; a hard clip at ±1.0 is the honest truth.

### 4.2 Failure modes
- Encoder returns error → drop frame, log, continue. One dropped 20 ms frame is inaudible.
- Encoded buffer > 4000 bytes → bug in params; assert in debug, drop in release.

---

## 5. Decoded voice → mixer routing

### 5.1 The VoiceBus (new)

Buses after this change:

| Bus | Headroom | Priority | Ducks? |
|-----|----------|----------|--------|
| `music` | −6 dB | 3 | ducks under voice and dialogue |
| `sfx` | 0 dB | 2 | ducks under voice |
| `dialogue` (scripted VO) | 0 dB | 1 | never ducks |
| **`voice` (proximity chat, NEW)** | 0 dB | 1 | never ducks |
| `ambient` | −9 dB | 4 | ducks under voice |

- `voice` and `dialogue` are sibling priority-1 buses. They do not duck each other — if a scripted line plays while a teammate speaks, both come through; that is the honest state of the world.
- Bus summing: pure function `mix_buses(array<Bus>) -> stereo_frame`.

### 5.2 VoiceSource lifecycle

One `VoiceSource` per remote speaker entity. N simultaneous speakers — hard cap 16 per listener (priority-sorted by distance if exceeded; distant 17th is dropped, not ducked).

```
VoiceSource {
    entity_id,
    jitter_buffer,        // owned by netcode side; we read decoded frames
    spatial_params,       // updated each game frame from transform + listener
    lpf_state,            // one-pole filter state (2 floats, L/R)
    gain_envelope,        // current smoothed gain
}
```

Pure mix function per source per 20 ms frame:
```
stereo_frame voice_source_render(const float* mono_in_960,
                                 const SpatialParams& sp,
                                 LpfState& lpf);
```
Writes to the `voice` bus accumulator. LPF state is the only mutable input, and it's owned by the source, not global.

---

## 6. 3D spatialization

HRTF-lite: distance attenuation + equal-power stereo pan + distance/occlusion low-pass. No convolution HRTF (that needs its own council vote and assets).

### 6.1 Distance attenuation

Inverse-square intensity, clamped to a sane voice range.

Let `d` = distance from listener to speaker, `d_min` = reference distance, `d_max` = cutoff.

**Derivation.** Sound intensity from a point source falls as `1/d²` (energy over expanding spherical shell of area `4πd²`). Amplitude is √intensity, so **amplitude ∝ 1/d**. But `1/d` diverges as `d → 0`; we clamp to `d_min`:

```
        ⎧ 1                           if d ≤ d_min
a(d) =  ⎨ d_min / d                   if d_min < d < d_max
        ⎩ 0                           if d ≥ d_max
```

Parameters (voice-tuned, different from SFX):
- `d_min = 1.0 m`   — inside this bubble you're "talking right next to them."
- `d_max = 25.0 m`  — voice is inaudible past this. Shorter than music/SFX, because whisper-range social presence, not broadcast.

A gentle `cos²` taper in the last 5 m before `d_max` avoids the cliff:
```
if d > d_max - 5:  a(d) *= cos²( (d - (d_max - 5)) / 5  ·  π/2 )
```

### 6.2 Stereo pan from azimuth — equal-power law

Listener has a forward vector **f** and right vector **r** (from camera). Speaker's listener-relative position is **p**. The azimuth angle θ (signed, left negative) satisfies:

```
θ = atan2( dot(p, r), dot(p, f) )
```

**Equal-power (constant-power) pan.** Mapping azimuth to L/R gains, we want `L² + R² = 1` so perceived loudness is constant as the source pans. Let `φ = θ/2 + π/4`, so φ ranges over `[0, π/2]` for θ in `[-π/2, +π/2]`. Then:

```
L = cos(φ),   R = sin(φ)
```

Check: φ = π/4 → L = R = √2/2, sum of squares = 1. φ = 0 → full left. φ = π/2 → full right. ✓

For rear-hemisphere sources (|θ| > π/2) we fold: clamp φ to `[0, π/2]` and additionally apply a −3 dB "behind" attenuation + a 1 kHz LPF cue. This is the HRTF-lite front/back disambiguation — cheap, not perfect, honest.

### 6.3 Distance/occlusion lowpass

Air absorbs high frequencies with distance; walls kill them harder.

One-pole LPF (RC-derived):
```
y[n] = α · x[n] + (1 − α) · y[n−1]
α = dt / (RC + dt),   fc = 1 / (2π RC)
```

Map distance and occlusion to cutoff:
```
fc(d, occ) = clamp(
    20000 Hz  −  (d / d_max) · 12000 Hz       // distance air absorption
             −  occ · 6000 Hz,                // occlusion (0..1 from raycast; phase 2)
    700 Hz,  20000 Hz
)
```

At d=0, no occlusion → 20 kHz (effectively bypass). At d=25 m, no occlusion → ~8 kHz (slightly muffled). At d=25 m through a wall (occ=1) → ~2 kHz (muffled). Floor at 700 Hz keeps intelligibility.

**Phase 1** ships with `occ = 0` always. **Phase 2** raycasts listener→speaker against `CollisionSystem` and integrates occlusion. Deferred to avoid coupling voice ship to physics query.

### 6.4 Doppler — deferred

Doppler on voice is uncanny and rare (you'd need the speaker moving faster than ~10 m/s relative to listener). Not worth the design cost at phase 1. Revisit if a vehicle demo ever ships.

---

## 7. Ducking rules

"Music should bow slightly when a human speaks. Never the other way around."

### 7.1 Envelope

Per-bus ducker is a pure function of (input frame, target gain, envelope state):

```
duck_state.gain_db → target_db
  attack:  50 ms  when target drops (duck engaging)
  release: 400 ms when target rises (duck disengaging)
```

Asymmetric on purpose: **fast duck in** (don't step on the first syllable), **slow duck out** (don't pump between words).

One-pole envelope with separate α:
```
α_attack  = 1 − exp(−dt / τ_attack),   τ_attack  = 50 ms
α_release = 1 − exp(−dt / τ_release),  τ_release = 400 ms
```

### 7.2 Duck amounts

When **any** VoiceSource with `attenuation > 0.01` (i.e. audible in listener range) is active:
- `music` bus: target −6 dB
- `sfx` bus: target −3 dB
- `ambient` bus: target −4 dB
- `dialogue` bus: **no duck** (VO and voice chat coexist)
- `voice` bus: **no self-duck**

When no voice is active: all targets ease back to 0 dB over 400 ms.

### 7.3 Interaction with `dialogue`

If both a VO line and a chat voice are active: both play, neither ducks the other, music is ducked by `max()` of the two duckers (so −6 dB, not −12 dB). `max_duck` is a pure reduction across active duckers per bus.

---

## 8. Latency budget

| Stage | Budget |
|-------|--------|
| WASAPI capture (1 frame) | 20 ms |
| VAD + encode (Opus, 20 ms frame) | ~5 ms |
| Transport RTT (LAN) | 5–20 ms |
| Jitter buffer (netcode owned) | 60 ms |
| Decode | ~3 ms |
| Spatialize + mix | ~1 ms |
| WASAPI render (1 frame) | 10 ms |
| **Total (LAN)** | **~104–120 ms** |

Target: **under 150 ms end-to-end on LAN**. This budget hits that.

**Pessimism flag.** The 60 ms jitter buffer is netcode's floor; WAN conditions will push it to 80–120 ms, lifting total to 170–220 ms. That is acceptable for social proximity chat. It is **not** acceptable for competitive call-outs — if we ever ship competitive, we revisit the jitter floor and possibly exclusive-mode render (saves ~7 ms).

The 20 ms WASAPI capture frame is the irreducible floor. Going to 10 ms doubles the packet rate for <10% perceptual benefit.

---

## 9. Privacy and UX

- **Mic monitor (local sidetone).** OFF by default. This is the "hear yourself in your headphones" feature. Enabling it on speakers causes a feedback loop; enabling it on an open-air mic causes echo. Make the user opt in through settings with a clear warning.
- **Per-player mute.** Client-side list of muted peer IDs persisted to `%APPDATA%/OdysseyEngine/mutes.json`. A muted peer's VoiceSource is created but its output is zeroed before mixing (we still decode — cheaper than tracking a gap in the stream; revisit if we ever have 32+ speakers).
- **Push-to-talk vs open-mic.** Default PTT (key: `V`). Open-mic is an opt-in toggle. Reason: VAD is good, not perfect, and new users on poor mics spam the channel.
- **Mic indicator.** While `vad_is_active == true` and transmitting, show a small mic icon on the local HUD. No icon = no transmission. Honesty over cleverness.
- **Spatial cue.** A speaker's own floating name-tag pulses faintly with their VAD envelope (remote listeners see it). Reinforces "who is talking" without clutter.

---

## 10. Integration with `mixer-dump`

The existing `mixer-dump` skill prints buses, sources, and the MusicDirector state. Voice must appear:

```
Bus voice       gain   0.0 dB  (linear 1.000)  active sources: 2

=== VoiceBus ===
  speaker 17 "Alice"    dist=4.2 m   atten=0.238   pan L=0.88/R=0.47   lpf=18200 Hz   gain=-12.5 dB
  speaker 42 "Bob"      dist=18.1 m  atten=0.055   pan L=0.32/R=0.95   lpf= 8900 Hz   gain=-25.2 dB
  speaker 99 "Carol"    MUTED (client list)
```

Required additions to `Mixer::dump_state`:
- `voice` bus line with active-source count.
- `VoiceBus` subsection enumerating each `VoiceSource`: entity/player name, distance, `a(d)`, per-ear pan, LPF cutoff, final gain, muted flag.

No change to the existing snapshot JSON schema except adding the `voice_bus` array alongside `sources`.

See also: new `voice-mix-preview` skill for the target-listener-centric view.

---

## 11. Phasing

1. **Phase 1 (ship).** WASAPI capture, VAD, Opus handoff, VoiceBus, distance attenuation, stereo pan, distance LPF (occlusion=0), ducking, mute list, PTT. `mixer-dump` updated. No raycast occlusion, no Doppler, no HRTF.
2. **Phase 2.** Occlusion raycasts through `CollisionSystem`, sidetone, VoiceBus spill into `music`-layer EQ notch (see KB §11.3).
3. **Phase 3 (if needed).** Minimum-phase HRTF convolution, Doppler, denoise. Each earns its own council vote.

---

## 12. Council triggers

This design contains several council-vote triggers:
- **New subsystem** — `src/audio/voice/` will be a new directory → council.
- **New dependency** — `libopus` not in `vcpkg.json` → council (Mandate #4).
- **New public API** — `Mixer::add_voice_source`, `Mixer::dump_state` signature extension.

Convene `/council` before implementation begins. Vote topic: "Proximity voice chat layer as specified in this doc + libopus as a grandfathered dep justified by the complexity of writing a speech codec from scratch."
