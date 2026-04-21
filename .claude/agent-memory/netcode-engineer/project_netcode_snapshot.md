---
name: Project netcode state snapshot (2026-04-20)
description: What is actually implemented in src/net/ today and what is still stubbed, so future sessions don't re-audit from zero
type: project
---

State of `src/net/` as of 2026-04-20 (verify with git log before acting on it):

**Implemented:**
- `socket.{h,cpp}` — Winsock UDP wrapper, non-blocking, broadcast for LAN disc.
- `protocol.{h,cpp}` — PROTOCOL_ID `0x4F445953` ("ODYS"), PROTOCOL_VERSION=1, MAX_PACKET_SIZE=1200, 16-byte header (seq/ack/ack_bits/type). PacketWriter/PacketReader byte-level, no bitpacking yet. Float quaternions (not smallest-three). Entity snapshot is full floats — no quantization.
- `server.{h,cpp}` — 60 Hz tick, 20 Hz snapshot default. ClientSlot tracks last_ack + ack_bits per peer. No per-client interest set yet.
- `client.{h,cpp}` — connect/disconnect, pending_inputs deque (prediction groundwork), rtt_/packet_loss_ fields but no estimation logic yet.
- `replication.{h,cpp}` — compute_snapshot_delta, apply_snapshot_delta, interpolate_snapshots, SnapshotBuffer for render-time interp. Delta uses thresholds (pos 0.01, rot 0.001, health 0.1).
- `lobby.{h,cpp}` — LAN discovery.

**NOT yet implemented (gaps the rubric calls out):**
- Bitpacking / quantization (positions still 12-byte vec3, quats 16-byte).
- Lag compensation / server-side rewind.
- Interest management (relevancy/PVS/grid).
- Input redundancy (last-N-inputs-per-packet).
- RTT estimation (Jacobson/Karels) — field exists, no update logic.
- Fragmentation (relies on MAX_PACKET_SIZE=1200).
- Per-tick snapshot hash for desync detection.
- Protocol version diff tooling.

**Why:** netcode is Phase 5 of the project plan; gaps are acknowledged. Editor Network Panel is the forcing function for the telemetry side; the snapshot + lag-comp side is on the roadmap but not gated by a demo yet.

**How to apply:** Before touching `src/net/`, `git log -- src/net/` to catch any drift. Don't assume bitpacking or lag-comp exists; the wire protocol is still v1 byte-packed.
