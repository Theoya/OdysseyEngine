# Proximity Voice Chat — Netcode Design

**Owner:** netcode-engineer (wire + transport + relay + interest)
**Co-owner:** marty-odonnell-composer (WASAPI capture, encode-to-bytes handoff, decode-to-playback)
**Status:** DESIGN — no C++ implementation yet.
**Protocol impact:** PROTOCOL_VERSION bump from 1 → 2. **Council vote required** (see §10).
**Date:** 2026-04-20

This document defines the wire format, transport model, interest filtering, jitter handling, bandwidth, and security for proximity voice in OdysseyEngine. It does **not** define WASAPI capture/playback internals — Marty owns that and hands the netcode layer encoded Opus frames + PCM sinks.

---

## 1. Requirements

| Concern | Target |
|---|---|
| Player count | Up to 8 (matches current `LANBroadcastPayload::max_players`) |
| Speaker concurrency | Realistically 2–3 simultaneous, worst case 8 |
| Latency budget (mouth→ear) | p50 ≤ 150 ms, p99 ≤ 250 ms end-to-end |
| Fairness | Server-authoritative, listener-side attenuation is cosmetic |
| Cheat resistance | Spoofing another player's voice must be infeasible |
| Platform | Windows only (WASAPI, Winsock) |
| Integration | Must ride existing `UDPSocket` + `PacketHeader` — no new transport |

Derived: voice packets must fit inside `MAX_PACKET_SIZE = 1200` bytes with headroom for the existing 16-byte `PacketHeader`. One frame per packet, no fragmentation.

---

## 2. Problem classification

This is a **new unreliable channel** on top of the existing UDP stack, plus a **server-side interest filter** that reuses the per-peer relevance set the replication layer will eventually maintain. It is *not* a new transport, is *not* a new service, and intentionally does *not* introduce P2P — see §4.

---

## 3. Codec: Opus

### Recommendation: Opus, 24 kbps, 20 ms frames, complexity 5, VOIP mode

Rationale:

- **Frame size 20 ms.** At 48 kHz mono that is 960 samples. A 24 kbps CBR encoding produces ≈ 60 bytes payload per frame. Well under the 1200 B MTU — no fragmentation, ever.
- **Bitrate 24 kbps.** Opus VOIP mode at 24 kbps is within its "fullband speech, near-transparent" envelope per RFC 6716 §2.1.1. Lower (≤16 kbps) loses presence; higher (32–64 kbps) buys little for speech and eats bandwidth budget we need for snapshots.
- **Complexity 5 (of 10).** Halves encode CPU vs. complexity 10 at a barely measurable quality cost. Speech-band quality is dominated by the bitrate, not the search depth.
- **CBR not VBR.** Predictable packet size simplifies bandwidth accounting and NAT/firewall heuristics. Variable-size voice packets also interact poorly with naive congestion-flagging middleboxes.
- **DTX off by default.** Comfort-noise generation is a playback concern; on the wire we prefer "silence = no packet" driven by PTT/VAD (§9), which is simpler and saves bandwidth unconditionally.

### Mandate #4 posture on the Opus dep

Mandate #4 says "no third-party library we can't explain line-by-line". Opus is a ~50 kLOC codec; nobody on the team is going to read every line of SILK + CELT. Two paths, pick one at the council vote:

**Option A — Grandfather Opus in.** Precedent: pugixml, shaderc, glm, VMA, ImGui are already grandfathered because the cost of rewriting them is vastly greater than the value, and their interfaces are narrow and well-understood. Opus fits the same mold: narrow `opus_encode` / `opus_decode` surface, IETF-standardized (RFC 6716), MIT-style BSD license, battle-tested in Discord/Mumble/WebRTC. Recommended: add `opus` to `vcpkg.json`, grandfather it in the council minutes, and pin the version.

**Option B — Write a minimal subset from first principles.** Rejected. Speech coding is a 30-year research field; a first-principles clone is a multi-month detour that will ship worse quality than Opus at 24 kbps.

**Option C — Restrict to Opus's SILK-only mode via a thin shim, then write our own CELT later if we ever need music-band quality.** Over-scoped for proximity voice, which is speech-only.

**Recommendation: Option A (grandfather).** Raise it explicitly at the /council vote for PROTOCOL_VERSION bump — if Marty or the architect push back, fall back to the dep vote separately.

---

## 4. Transport model: server relay, not P2P

### Options considered

| Option | Description | Verdict |
|---|---|---|
| **A. Server relay** | Clients send voice to server; server fans out to interest-filtered listeners | **Recommended** |
| B. Full-mesh P2P | Each client sends voice directly to every other client | Rejected |
| C. Hybrid (P2P for same-room, relay otherwise) | Complexity without clear win for 8-player cap | Rejected |

### Why server relay wins here

1. **Authority alignment.** The server already owns entity positions (AI is GPU-server-authoritative per `project_authority_model.md`). Only the server knows authoritatively which listeners are in range of which speaker. P2P would require leaking position state to clients faster and more completely than the existing replication pipeline does, which *breaks* the anti-wallhack interest model.
2. **Anti-cheat.** Server validates `speaker_entity_id` against the authed sender (§8). In P2P, a malicious client can forge the source entity and nothing stops them.
3. **NAT.** 8-player lobbies on Windows over LAN *mostly* work P2P; over the internet, full-mesh P2P hits NAT hole-punching failure modes that a central relay sidesteps completely. We already have a server, use it.
4. **Bandwidth locality.** Server relays only to listeners in range, so a speaker talking to one nearby listener costs ~60 B/20 ms out, not 7 × 60 B/20 ms. The server's upstream is the only cost, which matches the existing asymmetric bandwidth posture (server >> clients).
5. **Recording / moderation / replay.** Future feature — server sees every frame, can write to a ring buffer for report-a-player flows without any client cooperation.

Tradeoff accepted: one extra hop adds ~RTT/2 latency. At LAN it's 1–5 ms, at internet scale it's 20–40 ms. Still inside the 150 ms p50 budget.

---

## 5. Packet format

### New packet types (v2)

```
VOICE_FRAME    = 40   // client → server, and server → client (relay)
VOICE_CONTROL  = 41   // PTT state, mute, codec params (reliable-ish, reserved)
```

### VOICE_FRAME payload (sits after the existing 16-byte PacketHeader)

```
Offset  Size  Field               Notes
  0      4    speaker_entity_id   EntityID (u32). Server overwrites on relay with the
                                  authed sender's controlled entity — clients cannot set
                                  this on the outbound path, see §8.
  4      2    voice_sequence      u16, per-speaker, wraps. Separate from PacketHeader.sequence
                                  because voice cadence (50 Hz) differs from game tick.
  6      1    frame_ms            u8. 20 today. Reserved 10/40/60 for future.
  7      1    flags               bit0 = VAD_ACTIVE   (speaker believes speech present)
                                  bit1 = PTT_HELD     (push-to-talk currently down)
                                  bit2 = FEC_PRESENT  (reserved; Opus in-band FEC, later)
                                  bit3 = END_OF_TALK  (speaker just released PTT — tells
                                                       listener to flush jitter buffer)
                                  bit4-7 reserved, must be 0 in v2.
  8      N    codec_payload       Opus frame bytes. 24 kbps × 20 ms ≈ 60 B typical.
```

Total on-wire per voice packet:
`20-byte IP + 8-byte UDP + 16-byte PacketHeader + 8-byte voice header + ~60 B payload ≈ 112 B`.

### MTU / fragmentation policy

- `MAX_PACKET_SIZE = 1200`. Voice packets at 112 B are 10× under that ceiling.
- **Never fragment voice.** If a codec config ever produces a payload that would exceed 1200 B after the voice header, drop the frame and log — do not split. Voice frames are unreliable and time-valued; a reassembly path adds complexity with zero upside for speech at sane bitrates.
- Enforced at encode time by Marty's layer: `opus_encode` output length checked before handing to the netcode layer.

### Why not bitpack the voice header?

Voice header is 8 B. Bitpacking saves maybe 2 B per frame. At 50 frames/s that's 100 B/s — trivial vs. the 3 KB/s codec payload. Byte-aligned wins for debugger readability and matches the existing `PacketWriter` conventions.

---

## 6. Interest / culling

Voice interest **reuses** the per-peer relevance set the replication layer will produce (currently a TODO — see netcode snapshot memo). Until that lands, voice uses a simple per-tick distance query.

### Per-speaker radius

Each entity that can speak carries a float `voice_range` in its `stats` SSBO (already an SoA buffer on the GPU side). Default 20.0 m. Design intent:

- A whisper in proximity chat is ≤ 5 m.
- A normal voice is ≤ 20 m.
- A shout / radio is a separate channel (not this feature).

### Gating rule (per server tick, per speaker→listener pair)

Let `d = |speaker.pos - listener.pos|`.
Let `R = speaker.voice_range`.
Let `H = 2.0 m` (hysteresis band).

```
State machine per (speaker, listener) pair, stored server-side:
  OUT_OF_RANGE:
      if d ≤ R       → enter IN_RANGE,  begin relaying
  IN_RANGE:
      if d > R + H   → enter OUT_OF_RANGE, stop relaying,
                       send a final VOICE_FRAME with END_OF_TALK bit set
                       so the listener's jitter buffer flushes cleanly
```

Hysteresis `H = 10 %` of range. This prevents range-edge packet flapping — without it, a listener jogging back and forth across `d = R` would get relay on/off every tick, which sounds like horrible chop.

**Derivation of `H`:** at a tick rate of 60 Hz and typical walking speed ~3 m/s, a player crosses 0.05 m per tick. A 2 m band gives 40 ticks (≈ 0.67 s) of damping, long enough that one footstep doesn't flap, short enough that intent-to-leave is respected. Tuneable at runtime; the 2 m value is the ship default.

### Integration point

Server's voice relay pass runs once per server tick (60 Hz). For each received `VOICE_FRAME`:
1. Validate sender → entity binding (§8).
2. Query listener list from replication's relevance set **intersected** with the `d ≤ R + H` test.
3. Fan out, rewriting `PacketHeader` per destination (sequence is per-peer) but leaving the voice header (speaker_entity_id, voice_sequence, frame_ms, flags, codec_payload) **untouched** — the payload is bit-exact relayed. No server-side decode, no transcode, no repack.

Relaying without touching the codec payload keeps server CPU at ~zero per voice frame and preserves the end-to-end invariant: the bits the speaker encoded are the bits the listener decodes.

---

## 7. Jitter buffer (client side)

### Design: 3-packet adaptive ring, drop-oldest on overflow

At 20 ms frames, a 3-packet buffer is 60 ms of added latency — comfortably inside the 150 ms p50 budget and enough to mask 2-in-a-row packet loss plus typical wifi jitter.

```
Per-speaker state on the listener:
  ring[3]            // slots of (voice_sequence, Opus frame bytes or empty)
  next_seq_to_play   // u16, wrap-aware
  armed              // bool, true after first packet arrives, false after timeout
```

### Rules

1. **On receive:**
    - Compute `delta = int16_t(voice_sequence - next_seq_to_play)`.
    - If `delta < 0` → late, **drop** (arrived after its play slot).
    - If `delta ≥ 3` → overflow, advance `next_seq_to_play` to `voice_sequence - 2` and **drop-oldest** slot. This favours recent speech over old speech, which is the correct call for voice (old speech is stale).
    - Else → insert at `ring[voice_sequence % 3]`.
2. **On playback tick (50 Hz to match 20 ms frames):**
    - Pop `ring[next_seq_to_play % 3]`.
    - If present → decode via `opus_decode` → push PCM to WASAPI sink (Marty's domain).
    - If absent → `opus_decode(NULL, 0)` = **packet loss concealment** (Opus built-in, fills the gap with extrapolated audio).
    - Advance `next_seq_to_play`.
3. **Disarm:** if no frames for 500 ms, mark disarmed and stop calling decode. Next arriving frame re-arms and resets `next_seq_to_play`.
4. **END_OF_TALK flag:** immediately flush ring, disarm. Prevents a 60 ms tail of stale speech after PTT release.

### Why drop-oldest and not drop-newest

Voice is a *strict monotonic time series*. A 60 ms old speech fragment played after a fresh fragment sounds like a glitch. If we must drop, drop the past. Industry precedent: Mumble's `MumbleJitterBuffer`, Discord's Opus transport, and RFC 3550 RTP all converge on this.

### Why 3 and not adaptive / larger

3 is the sweet spot for LAN-to-regional play with 20 ms frames. A future enhancement: track observed jitter p99 per speaker, grow buffer to 4–5 slots if p99 > 30 ms. Ship with 3 fixed, add adaptive later once we have telemetry.

---

## 8. Security

### Threat model

1. **Voice impersonation.** Malicious client sends VOICE_FRAME with `speaker_entity_id = some_other_player`. If the server relays it, targets hear a spoofed voice.
2. **Replay / reorder attacks.** Attacker captures frames and replays to cause confusion.
3. **Bandwidth DoS.** Attacker floods the server with voice frames targeted at high-cost listener sets.
4. **Eavesdrop.** Attacker on the local network sniffs voice in cleartext.

### Mitigations

1. **Server-side rebinding of `speaker_entity_id`.** On ingress, server **ignores** the client-supplied `speaker_entity_id` and overwrites it with the entity bound to that authenticated client slot at accept time. Clients literally cannot spoof because the server never trusts the field from them; they send `speaker_entity_id = 0` and the server fills it in. Documented explicitly in packet-format comments so future changes don't silently reintroduce the bug.
2. **`voice_sequence` window check.** Server keeps last-seen `voice_sequence` per client. Frames more than 32 units old are dropped as replays. Wrap handled the same way `PacketHeader.sequence` is.
3. **Rate limit.** Server enforces ≤ 60 voice frames/s per client (20 % over the nominal 50 Hz). Bursts beyond that are dropped and logged. Caps DoS at ~7 KB/s per malicious client into the server.
4. **Per-session key (reserved, v2.x).** Packet payload XOR'd with a per-session keystream derived from the handshake. This is **not** a full transport-security solution (not AES-GCM, no forward secrecy), but it raises the bar from trivial sniffing to needing the session handshake. Full TLS-like crypto is deferred; it's a big dep and needs its own council vote. Flag for v2.1.

### What we do NOT do

- We do not sign voice frames. Per-frame Ed25519 would add ~64 B per 60 B payload. Not worth it.
- We do not encrypt in v2.0. Proximity voice on LAN is the initial target; WAN deployment gets the encryption layer.

---

## 9. PTT / VAD / open-mic

### Default: Push-to-talk (PTT)

- Binding: configurable key (default `V`). Engine InputManager already tracks keys.
- When held, capture flows; when released, capture stops and one `END_OF_TALK` flagged frame is sent.
- Rationale: PTT is the only mode that is unambiguously kind to other players. It also saves bandwidth and avoids false-positive speech on background noise.

### Optional: VAD (voice activity detection)

- Toggle in settings (`voice_mode = ptt | vad | open`).
- VAD decision lives in Marty's capture layer, not here. From the netcode layer's view, VAD mode behaves *exactly* like PTT: speech detected → frames sent with `VAD_ACTIVE` bit set; silence → no frames. The `PTT_HELD` bit is cleared in VAD mode so listeners can tell the two apart if UX wants to show it.

### Optional: Open-mic

- No gating, frames flow continuously while `voice_mode = open`. Bandwidth-expensive; only useful for private lobbies. Exposed but not default.

---

## 10. Bandwidth budget

### Per-speaker outbound (client → server)

- 1 voice packet every 20 ms = 50 pps.
- Per packet: 112 B wire.
- **Per-speaker upstream: 5.6 KB/s.**

### Server outbound (relay, worst case N listeners per speaker)

With 8-player cap and everyone in range of everyone (small-map worst case): 1 speaker × 7 listeners = 7 × 112 B × 50 pps = **39.2 KB/s per active speaker**.

With 3 concurrent speakers (a realistic chaotic moment): **≈ 118 KB/s server downstream**.

### Comparison to existing snapshot bandwidth

Existing: 20 Hz snapshot, ~600 B typical (per `net-stats-dump` format) = ~12 KB/s per client. For 7 clients: ~84 KB/s server downstream on snapshots alone. Voice adds ~40% on top in the 3-speaker case. **Acceptable.**

Voice interest filtering (§6) keeps this bounded: listeners out of range don't receive the speaker's frames. In the typical case (1 speaker, 2–3 in-range listeners), server cost is 15–25 KB/s per active talker.

### Per-client inbound (listener)

One voice stream at 5.6 KB/s. In a busy moment with 3 audible speakers = 17 KB/s. Well under any broadband floor.

---

## 11. Council escalation

**This feature trips the netcode-owned council trigger:** protocol version bump (1 → 2, adds VOICE_FRAME + VOICE_CONTROL packet types, adds voice header after PacketHeader).

**Also trips:** new dependency (Opus via vcpkg) — see Mandate #4 discussion in §3.

### What to put in front of /council

1. `PROTOCOL_VERSION = 2`, new packet types `VOICE_FRAME = 40`, `VOICE_CONTROL = 41`.
2. New wire fields: 8-byte voice header (speaker_entity_id, voice_sequence, frame_ms, flags).
3. New dep: `opus` in `vcpkg.json`. Recommend grandfathering under the same precedent as pugixml / shaderc / VMA.
4. Server-relay architecture (not P2P). Anti-cheat posture reinforced: server always rebinds `speaker_entity_id`.
5. Interest filter reuses replication relevance set + per-entity `voice_range` float.
6. **Marty must sign off** on the capture/playback handoff contract (byte-exact Opus frames + sample-rate/channel agreement). His weight is 4 — he can single-handedly force escalation. Get him in the vote.

Use the `/protocol-diff` skill (once the engine-side stub exists) to render the v1→v2 wire diff for the council packet.

### Expected blockers
- Marty may want 48 kHz stereo for music pass-through. Counter: proximity voice is mono speech; music is the MusicDirector's channel, not this.
- Architect may push back on Opus. Counter: option A (grandfather) with a narrow shim, or option C (SILK-only) if full Opus is contentious.

---

## 12. Test plan (design-level; implementation later)

**Pure-function tests (Mandate #2: success + failure):**
- `serialize_voice_frame` / `deserialize_voice_frame` — roundtrip success, truncated-buffer failure, bad-flags failure.
- Jitter buffer state machine — ordered delivery (success), one-packet late drop (success/expected-drop), overflow drop-oldest (success), 500 ms silence → disarm.
- Interest hysteresis — crossing IN→OUT, OUT→IN, flapping inside band (no transitions).

**Integration tests:**
- Loopback: client → localhost server → same client. Verifies encode/wire/decode roundtrip.
- Two-client LAN: A speaks, B in range, C out of range. Assert C receives zero voice frames.
- Loss injection via `/inject-loss` at 5%, 10%, 20%. Assert jitter buffer uses PLC and audio doesn't desync.

**First-principles math checks (Mandate #3):**
- Hysteresis derivation (above) is the whiteboard-derivable math. Noted in code comment at the gate site.
- Bandwidth numbers derive from: `bps × frame_ms / 8` = payload B/frame. Opus 24000 × 0.020 / 8 = 60 B. Anyone on the team can reproduce the arithmetic.

---

## 13. Open questions

1. **Text chat reuse.** Could the voice packet type be reused for text? No — text wants reliable-ordered, voice wants unreliable. Keep separate.
2. **Spectator mode.** Spectators hear everyone (no proximity)? Default yes, but mark explicitly in v2.1.
3. **Team chat channel.** Out of scope for proximity. Separate channel, separate interest rule, separate future RFC.
4. **Recording / server-side capture.** Out of scope. Design preserves the possibility (server sees every frame) without committing to it.

---

## 14. References

- RFC 6716 — Definition of the Opus Audio Codec. The interop spec for frame sizing and bitrate behaviour.
- RFC 3550 — RTP: A Transport Protocol for Real-Time Applications. Jitter buffer and sequence number conventions are direct descendants.
- Valve's Source Multiplayer Networking (Yahn Bernier) — the interest/relevance model we're mirroring for voice culling.
- Glenn Fiedler, "Reliability and Flow Control" / "Virtual Connection Over UDP" — the ack-bitfield pattern already in `PacketHeader` carries over cleanly.
- Mumble protocol (mumble.info/documentation) — closest production example of Opus-over-UDP with jitter buffer; independent confirmation of the 3-slot, drop-oldest design.
- Discord's "How Discord Handles Two and Half Million Concurrent Voice Users" engineering blog — confirms server-relay for voice at scale for anti-cheat and NAT reasons.
