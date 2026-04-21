# Showcase Demo — Stingers Brief

A stinger is a short composed cell that rides over the bed, punctuating a narrative beat. It is not a sound effect. It is a sentence of music with a verb.

All stingers are authored at **48 kHz / 24-bit PCM WAV**, bus-routed to the `stinger` bus, which sits above `music_bed` in the priority ladder and ducks the bed by -4 dB for the stinger's duration + 180 ms release.

File layout:

```
demo/showcase/music/stingers/
  combat_onset.wav
  boss_sighted.wav
  victory_fanfare.wav
  ally_down.wav
  treasure_found.wav
  dread_reveal.wav
```

Every stinger specifies a sync mode. The MusicDirector queues the stinger until the next sync boundary of the currently playing state, then fires it sample-aligned. A stinger that fires mid-beat is a broken sync_mode — see the design doc acceptance section.

---

## combat_onset — "the blade is drawn"

- **Context:** explore → combat transition (sync=bar, bridge_bars=2)
- **Duration:** 1.5 s
- **Tempo / key:** 128 BPM, D phrygian (destination-state-aligned)
- **Instrumentation:** shime-daiko triple-strike (beats 1, 1.5, 2) on `don-go-don`; low brass unison on D2 swell; cymbal swell choke on beat 4
- **Melodic content:** none — this is percussion + drone. The rhythm carries it.
- **Dynamics:** pp crescendo into ff on beat 4
- **Mix notes:** transient-forward, no reverb tail past 800 ms (the combat bed takes over)
- **Sync mode:** bar — must land on bar 1 of combat state

## boss_sighted — "the mountain has eyes"

- **Context:** fired via `/stinger-fire boss_sighted` when a boss archetype enters the player's awareness (typically while already in combat state)
- **Duration:** 2.4 s
- **Tempo / key:** 96 BPM, D phrygian (slower than combat tempo — the moment stretches)
- **Instrumentation:** men's choir TTBB on a sustained open 5th (D2 + A2) with a slow rise to a minor 9th cluster (D2, A2, Eb3); subterranean tuba on D1; a single bowed crotale ring on the tritone (Ab5)
- **Melodic content:** no melody — this is a held chord that opens like a mouth
- **Dynamics:** pp entry, swell to f over 1.8 s, then sudden drop for the last 600 ms
- **Mix notes:** cathedral reverb 4.5 s (bleeds into the following bar of combat)
- **Sync mode:** bar

## victory_fanfare — "it is done"

- **Context:** combat → victory transition (sync=phrase, bridge_bars=4)
- **Duration:** 3.2 s
- **Tempo / key:** 96 BPM, A major (destination-state-aligned, tierce de Picardie setup)
- **Instrumentation:** full brass (4 horns, 3 trumpets, 2 trombones, tuba) on the plagal motion D → A; timpani on A pedal rolling; cymbal shimmer
- **Melodic content:** trumpets state the first three notes of the hero chant (A - B - A) in A major, resolving to the victory state's downbeat
- **Dynamics:** f entry, building to ff on the cadence point
- **Mix notes:** hall reverb 3.2 s; the reverb tail bleeds into the victory state's bar 1
- **Sync mode:** phrase — must land on bar 1 of a fresh 8-bar phrase

## ally_down — "a friend is gone"

- **Context:** fired on ally death event, independent of current state (most commonly during combat)
- **Duration:** 2.0 s
- **Tempo / key:** 72 BPM, A aeolian (explore tempo — the moment stops the fight)
- **Instrumentation:** solo cello on a descending m3 fall (A3 → F3), tail held; solo horn echoes in the last 800 ms on F3; no percussion
- **Melodic content:** the exploration bed's chant cell, *inverted* — A rocks down to F and stays. The player has lost a friend; the theme has lost its answer.
- **Dynamics:** mp entry, decresc. to pp over 2 s
- **Mix notes:** cathedral reverb 3.5 s; this stinger intentionally hangs
- **Sync mode:** beat (close enough to the event to feel immediate, but musical)

## treasure_found — "the room remembers you"

- **Context:** fired on treasure pickup / discovery event, independent of current state
- **Duration:** 1.8 s
- **Tempo / key:** 72 BPM, A aeolian moving to A major (brief major mode mixture — a glimpse of victory's tierce)
- **Instrumentation:** harp glissando ascending C4 → A5; solo flute on a three-note pickup (E - A - C#5 — ending on the raised 3rd); a single celesta bell on A5
- **Melodic content:** a three-note lift, aeolian into parallel major — the musical equivalent of a lantern flaring
- **Dynamics:** p throughout, no swell — understated
- **Mix notes:** room reverb 2.2 s; the celesta bell carries the tail
- **Sync mode:** bar

## dread_reveal — "there is no floor"

- **Context:** explore → dread transition (sync=bar, bridge_bars=1)
- **Duration:** 2.6 s
- **Tempo / key:** 54 BPM, C locrian (destination-state-aligned)
- **Instrumentation:** detuned piano cluster on C1 + Db1 + Gb1 (the locrian triad pressure); a single bowed double bass harmonic on C2; a waterphone shudder entering at 1.5 s
- **Melodic content:** none — this is a chord that collapses. No motion after the initial strike.
- **Dynamics:** mp strike, immediate decay to pp; waterphone enters ppp and fades
- **Mix notes:** dry strike (no reverb on piano — we want the room to feel small); waterphone gets long plate 4.0 s
- **Sync mode:** bar

---

## Authoring checklist (per stinger)

- [ ] Duration matches `duration_ms` attribute in `showcase.music.xml`
- [ ] BPM/key compatible with both source and destination states (if transition)
- [ ] `/stinger-fire <id>` fires on the correct sync boundary (verify with `/beat-grid`)
- [ ] Reverb tail does not leak past the next state's bar 1 unintentionally
- [ ] Peak normalized to -3 dBFS (stingers are louder than the bed by design)
- [ ] 48 kHz / 24-bit PCM WAV
- [ ] `/mixer-dump` shows `stinger` bus active for the stinger's duration + tail
- [ ] Music bed visibly ducks by -4 dB during stinger playback
