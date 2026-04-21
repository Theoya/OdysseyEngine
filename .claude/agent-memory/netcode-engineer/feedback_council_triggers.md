---
name: Netcode council triggers I own
description: Changes that MUST auto-invoke /council before merge — my domain's gates
type: feedback
---

Auto-invoke `/council` before any of these land in `src/net/`:

1. **PROTOCOL_VERSION bump** in `src/net/protocol.h`.
2. **Tick rate or snapshot rate default change** in `ServerConfig`.
3. **Snapshot field add/remove/reorder** in `EntitySnapshot` (wire ABI break).
4. **Authority model flip** (e.g. client-auth → server-auth or vice versa) on any system.
5. **New RPC surface** (new PacketType added to the enum).
6. **Transport change** (switching off raw UDP, adding TCP fallback, enabling relay).

**Why:** these are the changes that (a) silently brick existing clients, (b) open cheat surfaces, or (c) blow bandwidth budgets. Council catches cross-discipline fallout — ai-engineer on AI replication shape, asset-engineer on schema churn, architect on build/memory, security implicit in protocol.

**How to apply:** if a PR touches any of the above and has no `docs/decisions/<date>-<slug>.md`, refuse it and call `/council` first. Not triggers: bugfixes to existing packet serialization that don't change the wire, diagnostic logging additions, test-only changes.
