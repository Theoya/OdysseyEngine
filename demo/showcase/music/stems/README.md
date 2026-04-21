# Showcase Demo — Music Stems Plan

All stems are authored at **48 kHz / 24-bit PCM WAV** (Windows-only, WASAPI device rate — no runtime SRC).
Every stem within a state shares exact BPM, key, bar count, and sample-accurate loop boundaries so the MusicDirector can sum them at any intensity level without phasing artifacts.

File layout:

```
demo/showcase/music/stems/
  explore/
    explore_72_Aaeolian_bed.wav
    explore_72_Aaeolian_mid.wav
    explore_72_Aaeolian_full.wav
  combat/
    combat_128_Dphrygian_ji.wav
    combat_128_Dphrygian_bass.wav
    combat_128_Dphrygian_harm.wav
    combat_128_Dphrygian_lead.wav
  victory/
    victory_96_Amajor_brass.wav
    victory_96_Amajor_choir.wav
    victory_96_Amajor_strings.wav
  dread/
    dread_54_Clocrian_drone.wav
    dread_54_Clocrian_piano.wav
    dread_54_Clocrian_metal.wav
```

Naming convention: `{state}_{bpm}_{key}_{stem_role}.wav`. Never drop the BPM and key — these are load-bearing metadata the MusicDirector verifies on asset load.

---

## EXPLORE (72 BPM, A aeolian, 16 bars, 4/4)

Total loop duration: 16 bars × 4 beats × (60 / 72) s = **53.333 s** exactly.

### explore_72_Aaeolian_bed.wav — intensity_min 0.00 (always audible)
- **Role:** drone + monastic solo voice cell
- **Instrumentation:** solo male voice on Latin open vowels ("o-a-e"); sustained A1/A2 pedal from bowed double bass + cello harmonic; sparse felt-hammer piano on the tonic A every 4 bars
- **Harmony:** A aeolian drone, no chord changes
- **Melodic content:** three-note chant cell `A3 - B3 - A3` stated once per 8 bars (monastic rocking)
- **Dynamics:** pp throughout, swelling to mp on bar 9 rocking
- **Duration:** 53.333 s loop, sample-accurate loop seam at bar 16 beat 4 + 1 sample
- **Mix notes:** low cut at 40 Hz, gentle plate reverb (2.8 s tail, pre-delay 40 ms)

### explore_72_Aaeolian_mid.wav — intensity_min 0.35
- **Role:** cello counterline + ambient harp
- **Instrumentation:** solo cello (C minor pentatonic fragment over A aeolian — shared pitches); ambient harp glissandi on bars 4, 8, 12, 16
- **Harmony:** implies i → bVII → i (Am → G → Am), no cadence
- **Melodic content:** cello moves `E3 - G3 - A3 - C4 - B3 - A3` (the "curiosity" line)
- **Dynamics:** p entering on bar 1 with 2-bar fade-in; crests mp at bar 8
- **Duration:** 53.333 s, loop-locked to bed

### explore_72_Aaeolian_full.wav — intensity_min 0.70
- **Role:** women's choir pad + solo flute on the leitmotif
- **Instrumentation:** 8-voice women's choir (SSA) on "ah"; solo concert flute stating the scout leitmotif (first-sight variant) in diminution
- **Harmony:** full modal triads — i, bIII, iv, bVII in A aeolian
- **Melodic content:** flute states a transposed scout motif at bar 5 and bar 13
- **Dynamics:** mp entering with 4-bar fade-in; never exceeds mf
- **Duration:** 53.333 s, loop-locked to bed
- **Mix notes:** choir gets cathedral reverb (4.2 s); flute stays drier (hall 1.8 s)

---

## COMBAT (128 BPM, D phrygian, 8 bars, 4/4)

Total loop duration: 8 bars × 4 beats × (60 / 128) s = **15.000 s** exactly.

### combat_128_Dphrygian_ji.wav — intensity_min 0.00 (always audible — the spine)
- **Role:** taiko jiuchi base beat — the heartbeat
- **Instrumentation:** shime-daiko on straight 16ths (`doko doko`); chu-daiko on beats 1 and 3
- **Harmony:** unpitched
- **Rhythmic content:** `[don _ go _ don _ go _] [don _ go _ don _ go _]` × 8 bars
- **Dynamics:** mf, perfectly consistent — this layer never modulates
- **Duration:** 15.000 s, loop-locked
- **Mix notes:** transient-preserving compression; high shelf +2 dB at 4 kHz for stick articulation

### combat_128_Dphrygian_bass.wav — intensity_min 0.20
- **Role:** low ostinato
- **Instrumentation:** contrabass + tuba unison; electric bass doubling; cello pizzicato on accents
- **Harmony:** D phrygian pedal (D) with Eb neighbor (the b2 fingerprint)
- **Melodic content:** `D2 - D2 - Eb2 - D2 | D2 - C2 - D2 - rest` (a 2-bar ostinato, stated 4×)
- **Dynamics:** mf entering with 1-bar fade-in
- **Duration:** 15.000 s, loop-locked

### combat_128_Dphrygian_harm.wav — intensity_min 0.50
- **Role:** harmony bed — strings + electric guitar power-fifths
- **Instrumentation:** tutti strings tremolo; distorted electric guitar on D5 + A5 power chords
- **Harmony:** Dm → Ebmaj (b2) → Cm → Dm — the phrygian turnaround
- **Melodic content:** strings provide sustained dyads; guitar punches on beat 3 of bars 2, 4, 6, 8
- **Dynamics:** mf entering with 2-bar fade-in
- **Duration:** 15.000 s, loop-locked
- **Mix notes:** guitar mid-cut at 800 Hz to leave room for lead trumpet

### combat_128_Dphrygian_lead.wav — intensity_min 0.80
- **Role:** lead melody — solo trumpet + choir stabs on the fight motif
- **Instrumentation:** solo Bb trumpet in high register; men's choir TTBB on short "ha!" stabs
- **Harmony:** follows harm layer
- **Melodic content:** states the brute leitmotif (hostile variant, inverted, on low brass transposed up an octave to trumpet); choir punctuates beats 1 and 3 of bars 5–8
- **Dynamics:** f, 2-bar fade-in
- **Duration:** 15.000 s, loop-locked
- **Mix notes:** trumpet gets plate reverb 2.0 s; choir is dry and forward

---

## VICTORY (96 BPM, A major, 8 bars, 4/4, plays once — final=true)

Total duration: 8 bars × 4 beats × (60 / 96) s = **20.000 s** exactly.

### victory_96_Amajor_brass.wav
- **Role:** full horns + trumpets on the plagal tail
- **Instrumentation:** 4 horns, 3 trumpets, 2 trombones, tuba
- **Harmony:** I - IV - I - V - I - IV - I (tierce de Picardie — the C# replaces the aeolian C natural; plagal D→A tail on bars 5–8)
- **Melodic content:** the explore chant cell restated in A major (C# instead of C), full brass unison at the octave, fanfare rhythm
- **Dynamics:** ff throughout, no fade
- **Duration:** 20.000 s, no loop
- **Mix notes:** hall reverb 2.8 s; no compression — let the dynamic swell breathe

### victory_96_Amajor_choir.wav
- **Role:** full SATB choir on open vowels, stating the hero cadence
- **Instrumentation:** 24-voice SATB
- **Harmony:** follows brass
- **Melodic content:** soprano carries the C# — the tierce de Picardie lift — on bar 1 beat 1
- **Dynamics:** ff, sustains through all 8 bars
- **Duration:** 20.000 s, no loop
- **Mix notes:** cathedral reverb 4.5 s; air shelf +2 dB at 10 kHz for sheen

### victory_96_Amajor_strings.wav
- **Role:** tutti strings legato
- **Instrumentation:** full string orchestra (16-14-12-10-8)
- **Harmony:** follows brass
- **Melodic content:** violins double the chant; violas + cellos carry inner voices; basses on root motion
- **Dynamics:** ff, with gentle vibrato swell
- **Duration:** 20.000 s, no loop

---

## DREAD (54 BPM, C locrian, 12 bars, 4/4)

Total loop duration: 12 bars × 4 beats × (60 / 54) s = **53.333 s** exactly (same as explore — intentional).

### dread_54_Clocrian_drone.wav — intensity_min 0.00
- **Role:** low drone + sub-bass pedal
- **Instrumentation:** bowed double bass on C1; contrabassoon on C2; sub-synth sine on C0 (gentle, -20 dB)
- **Harmony:** C locrian drone — no chord motion
- **Dynamics:** pp, breathing in 8-second swells
- **Duration:** 53.333 s, loop-locked
- **Mix notes:** hi-pass at 30 Hz to control sub; no reverb (dryness IS the feeling)

### dread_54_Clocrian_piano.wav — intensity_min 0.25
- **Role:** prepared piano on inharmonic nodes
- **Instrumentation:** grand piano with screws and rubber wedges on strings at harmonic nodes
- **Harmony:** clusters on C, Db, Gb (the locrian fingerprint — the diminished 5th)
- **Melodic content:** single attacks every 3–5 beats, seemingly random (composed to avoid implying pulse)
- **Dynamics:** p, sparse
- **Duration:** 53.333 s, loop-locked (the "randomness" is fully composed)

### dread_54_Clocrian_metal.wav — intensity_min 0.60
- **Role:** waterphone + bowed crotales
- **Instrumentation:** waterphone (long bowed notes with microtonal wobble); bowed crotales on the Gb (the tritone)
- **Harmony:** microtonal inflection around Gb
- **Dynamics:** pp–mp, breathing swells that peak at bar 4, 8, 12
- **Duration:** 53.333 s, loop-locked

---

## Authoring checklist (per stem)

- [ ] BPM matches filename exactly
- [ ] Key matches filename exactly
- [ ] Bar count matches filename's implied duration (check with `/beat-grid`)
- [ ] Loop seam is sample-accurate (zero-crossing at last sample)
- [ ] All stems within a state sum musically at every intensity combination
- [ ] No stem contains a cadence that would be "wasted" — cadences belong to the transition stingers and to victory only
- [ ] 48 kHz / 24-bit PCM WAV
- [ ] Peak normalized to -6 dBFS (leaves headroom for the master bus)
- [ ] `/waveform <stem>` shows expected envelope, loop markers at bar boundaries
