---
name: Protocol v2 proposal — proximity voice
description: Design doc landed for PROTOCOL_VERSION bump 1 -> 2 adding VOICE_FRAME/VOICE_CONTROL, server-relay topology, Opus codec. Pending council vote.
type: project
---

Design doc at `docs/design/proximity_chat_netcode.md` (2026-04-20) proposes PROTOCOL_VERSION bump 1 -> 2 for proximity voice chat.

**Key decisions baked in:**
- Codec: Opus 24 kbps / 20 ms / complexity 5 / CBR / VOIP mode. ~60 B payload per frame. Grandfather under Mandate #4 precedent (pugixml/shaderc/VMA/ImGui).
- Topology: server relay, not P2P. Server rebinds `speaker_entity_id` on ingress — clients cannot spoof source.
- New packet types: `VOICE_FRAME = 40`, `VOICE_CONTROL = 41`.
- Voice header sits after PacketHeader: speaker_entity_id u32, voice_sequence u16, frame_ms u8, flags u8, payload[].
- Interest: per-speaker `voice_range` float on entity stats SSBO, hysteresis band H = 2.0 m, reuses replication relevance set.
- Jitter buffer: 3-slot drop-oldest ring, 50 Hz playback tick, Opus PLC on missing frames, END_OF_TALK flushes.
- Default PTT; VAD optional; open-mic reserved. Marty owns capture/playback (WASAPI).

**Why:** shooter demo wants proximity voice; doing it right up front prevents a second protocol bump later.

**How to apply:** before any VOICE_FRAME implementation work, call `/council` to vote on (a) the protocol bump, (b) Opus dep add, (c) Marty's capture/playback contract. Do not merge code until vote recorded in `docs/decisions/`. Use `/protocol-diff` (once engine stub exists) to render the v1->v2 wire diff for the council packet.

**Open items flagged in doc §13:**
- Spectator voice rules
- Team-chat channel (separate future RFC)
- Server-side capture/recording (design preserves possibility, no commitment)
- Per-session crypto deferred to v2.1
