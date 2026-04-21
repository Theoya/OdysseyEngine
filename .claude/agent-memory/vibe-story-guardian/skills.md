---
name: Skill surface (story-guardian)
description: The five Claude Code skills that the story-guardian owns, and when each runs
type: reference
---

Story-guardian's five skills live under `C:\Users\THadfield\.claude\skills\<name>\SKILL.md`.

**How to apply:**
- `/charter-init` — run ONCE per project to extract and codify the Vibe Charter from a structured interview. Refuses if a charter already exists; redirect to `/charter-edit`.
- `/charter-check <proposal>` — daily gate. Scores any felt-experience proposal against the charter and returns BLESSED / RESHAPE / REJECTED with pillar citations. This is the high-frequency skill.
- `/charter-edit` — tracked amendment flow. Soft edits (wording, touchstones) proceed normally; hard edits (pillar rewrite, One-Sentence Truth change, Forbidden list add/remove) require council vote.
- `/vibe-audit <subsystem>` — drift hunt, bounded to one subsystem. Produces ranked drift list (forbidden > structural > tonal > cosmetic) with per-item corrections.
- `/anti-touchstone-check <asset_path>` — single-asset smell test against the charter's anti-touchstones. Returns PASS (with pillar honored) or FLAG (with features triggering the match and a correction brief).

All skills read the charter from `C:\Users\THadfield\.claude\agent-knowledge\vibe-story-guardian\vibe_charter.md` first; fall back to the repo mirror at `T:\OdysseyEngine\docs\vibe_charter.md` only if agent-canonical is missing.
