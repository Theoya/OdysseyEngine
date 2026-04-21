---
name: OdysseyEngine authority model decisions
description: Who owns what over the wire — AI/Nadir, scripts, physics, player input — so I don't re-litigate these each session
type: project
---

**Facts (ratified in the council plan at `.claude/plans/please-build-an-editor-mutable-beaver.md`):**

1. **Nadir AI is server-authoritative, never peer-simulated.**
   - Why: Nadir runs on GPU compute. Different GPUs (driver version, warp size, fast-math flags) produce different float results. Non-deterministic across machines → can't lockstep.
   - How to apply: AI decisions replicate as `EntitySnapshot` state only. Clients never re-run the .nadir shader for replicated entities. If the user proposes peer-simulating Nadir, reject and cite this.

2. **Runtime-attached scripts are client-local only.**
   - Why: `ScriptRunner::attach_script` is a live-attach API (used by the Script Console). Accepting script attaches from the wire is an RCE surface.
   - How to apply: Server signs a script manifest at startup. Any script not in the manifest is rejected. Editor's Script Console is disabled during networked sessions. Never add a "replicate script attach" RPC.

3. **Physics authority requires fixed-point on server-authoritative paths.**
   - Why: Catto-style sequential impulses are float and therefore non-deterministic cross-platform. The council adopted first-principles rigid-body over Jolt partly because we control the numeric path.
   - How to apply: `docs/physics_authority.md` is the spec. Networked rigid bodies must use the fixed-point integrator path; decorative/client-only bodies can stay float.

4. **Favor-the-shooter hit reg (Overwatch/Source pattern), bounded to ~200 ms of rewind.**
   - Why: shooter feel; longer rewind enables "shot around corner" exploits.
   - How to apply: lag comp buffer sized for 200 ms. Anything older → reject the shot.
