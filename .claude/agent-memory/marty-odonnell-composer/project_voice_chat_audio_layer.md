---
name: Proximity voice chat audio layer
description: Decisions for the proximity-voice audio layer (WASAPI capture, VAD, VoiceBus, spatialization, ducking, EQ carve). Design doc at docs/design/proximity_chat_audio.md.
type: project
---

Proximity voice chat is a new audio bus layered onto the existing first-principles mixer. Design doc: `T:/OdysseyEngine/docs/design/proximity_chat_audio.md`. KB addendum (voice-as-ensemble / 2-4 kHz carve): `T:/OdysseyEngine/.claude/agent-memory/marty-odonnell-composer/kb_addendum_voice_as_ensemble.md`. Companion skill staged at `T:/OdysseyEngine/docs/skills/voice-mix-preview.SKILL.md` (canonical install path is `C:/Users/THadfield/.claude/skills/voice-mix-preview/SKILL.md`, sandbox blocked direct write).

**Why:** the authored-from-scratch audio subsystem must extend to handle multiplayer voice without importing middleware. Netcode owns the wire; the audio agent owns capture, playback, spatialization, and bus routing. Design must preserve the purity/lean mandate and `mixer-dump` diagnostics.

**How to apply:** when the user works on voice chat, reach for these fixed parameters before proposing alternatives:
- Capture: WASAPI shared mode, communications role, 48 kHz mono float32, 20 ms frames, `IAudioClient` with event-driven wake.
- VAD: RMS + ZCR dual-threshold hysteresis (open -38 dBFS / close -45 dBFS / min ZCR 0.02 / 200 ms hang).
- VoiceBus: new priority-1 bus, never self-ducks, no duck vs `dialogue`.
- Distance: `a(d) = d_min/d`, clamped at `d_min=1 m`, `d_max=25 m`, 5 m cos^2 taper before cutoff.
- Pan: equal-power from azimuth `theta = atan2(dot(p,right), dot(p,forward))`.
- LPF: one-pole, cutoff = 20 kHz minus 12 kHz (distance ratio) minus 6 kHz (occlusion). Floor 700 Hz.
- Ducking: music -6 dB, sfx -3 dB, ambient -4 dB when any audible voice source active. Attack 50 ms, release 400 ms. `dialogue` does not duck.
- Latency budget: target <150 ms LAN end-to-end (capture 20 + encode 5 + RTT + jitter 60 + decode 3 + render 10).
- EQ carve: peaking biquad on `music` bus at 3 kHz / Q=1 / -4 dB, sidechained to voice activity — on top of the -6 dB whole-bus duck.
- Mic monitor OFF by default. Push-to-talk `V` default. Client-side mute list in `%APPDATA%/OdysseyEngine/mutes.json`.
- libopus is a new dep; requires council vote before merge (Mandate #4).
- New subsystem dir `src/audio/voice/` and new public APIs (`Mixer::add_voice_source`, `VoiceBus::preview_for_listener`) require `/council` before implementation.
