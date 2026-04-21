// Server voice-relay tests.
//
// Uses a test seam — Server::set_voice_send_fn() — to capture relayed packets
// in-memory without opening real UDP sockets. Verifies the five council-
// mandated invariants of the voice relay:
//
//   1. Anti-spoof rebind: server overwrites client-supplied speaker_entity_id
//      with the authed sender's player_entity.
//   2. Interest filter: listener beyond voice_range receives no frames.
//   3. voice_range clamped to VOICE_RANGE_MAX_METERS (50m) on ingress.
//   4. Speaker never receives their own relay.
//   5. END_OF_TALK flagged frames are NOT relayed (silence suppression).

#include <gtest/gtest.h>

#include "audio/voice/voice_frame.h"
#include "net/protocol.h"
#include "net/server.h"

#include <cstring>
#include <tuple>
#include <vector>

using namespace odyssey;
using namespace odyssey::net;
using odyssey::audio::voice::VoiceFrame;
using odyssey::audio::voice::serialize_voice_frame;
using odyssey::audio::voice::deserialize_voice_frame;

namespace {

// Test fixture that populates a Server with three client slots + fake addrs
// without opening real sockets. It exposes a captured send log that each
// test can inspect.
struct RelayFixture {
    Server server;
    std::vector<std::tuple<Address, std::vector<uint8_t>>> sent;

    RelayFixture() {
        // Seed 3 connected clients at distinct entity ids / addresses.
        auto& clients = server.get_clients_mutable();
        clients.resize(3);
        for (size_t i = 0; i < 3; ++i) {
            clients[i].connected = true;
            clients[i].address.host = "192.0.2." + std::to_string(10 + i);
            clients[i].address.port = static_cast<uint16_t>(50000 + i);
            clients[i].player_entity = static_cast<EntityID>(1000 + i);
            clients[i].name = "p" + std::to_string(i);
            clients[i].voice.voice_range = 25.0f;
            clients[i].voice.position = vec3(0.0f);
        }
        server.set_voice_send_fn([this](const Address& d, const uint8_t* data, size_t n) {
            sent.emplace_back(d, std::vector<uint8_t>(data, data + n));
        });
    }

    // Build a valid VOICE_FRAME packet coming from a given sender. The client
    // supplies a (lying!) speaker_entity_id to verify the rebind.
    std::vector<uint8_t> make_ingress(uint16_t voice_seq,
                                      uint8_t flags,
                                      uint32_t client_supplied_entity = 0xDEAD,
                                      std::vector<uint8_t> payload = {0xAB, 0xCD, 0xEF})
    {
        VoiceFrame vf;
        vf.header.protocol_id = PROTOCOL_ID;
        vf.header.sequence = voice_seq;
        vf.header.type = PacketType::VOICE_FRAME;
        vf.voice.speaker_entity_id = client_supplied_entity;
        vf.voice.sequence = voice_seq;
        vf.voice.frame_ms = 20;
        vf.voice.flags = flags;
        vf.opus_payload = std::move(payload);
        return serialize_voice_frame(vf);
    }
};

} // namespace

TEST(VoiceRelay, AntiSpoofRebindsSpeakerEntityId) {
    RelayFixture fx;
    auto& clients = fx.server.get_clients_mutable();

    // Sender is slot 0, claiming to be entity 0xDEAD. Server must rewrite to
    // the actual player_entity = 1000.
    auto bytes = fx.make_ingress(/*seq=*/1, /*flags=*/0, /*fake=*/0xDEAD);
    fx.server.handle_voice_frame(clients[0].address, bytes.data(), bytes.size());

    // Should relay to slots 1 and 2 (both in range at origin).
    ASSERT_EQ(fx.sent.size(), 2u);
    for (const auto& [addr, wire] : fx.sent) {
        auto parsed = deserialize_voice_frame(wire.data(), wire.size());
        ASSERT_TRUE(parsed.is_ok());
        EXPECT_EQ(parsed.value().voice.speaker_entity_id, 1000u);
    }
}

TEST(VoiceRelay, SpeakerNeverReceivesOwnRelay) {
    RelayFixture fx;
    auto& clients = fx.server.get_clients_mutable();

    auto bytes = fx.make_ingress(1, 0);
    fx.server.handle_voice_frame(clients[0].address, bytes.data(), bytes.size());

    for (const auto& [addr, wire] : fx.sent) {
        EXPECT_NE(addr, clients[0].address);
    }
}

TEST(VoiceRelay, ListenerBeyondVoiceRangeDropped) {
    RelayFixture fx;
    auto& clients = fx.server.get_clients_mutable();

    // Slot 0 at origin, range 10 m. Slot 1 at 5 m (in), slot 2 at 20 m (out).
    clients[0].voice.voice_range = 10.0f;
    clients[0].voice.position = vec3(0.f);
    clients[1].voice.position = vec3(5.f, 0.f, 0.f);
    clients[2].voice.position = vec3(20.f, 0.f, 0.f);

    auto bytes = fx.make_ingress(1, 0);
    fx.server.handle_voice_frame(clients[0].address, bytes.data(), bytes.size());

    ASSERT_EQ(fx.sent.size(), 1u);
    EXPECT_EQ(std::get<0>(fx.sent[0]), clients[1].address);
}

TEST(VoiceRelay, VoiceRangeClampedToFiftyMetersOnIngress) {
    RelayFixture fx;
    auto& clients = fx.server.get_clients_mutable();

    // Attacker sets an absurd voice_range. set_client_voice_range hard-clamps.
    fx.server.set_client_voice_range(0, 9999.0f);
    EXPECT_LE(clients[0].voice.voice_range, VOICE_RANGE_MAX_METERS);

    // Listener at 75 m is outside the 50 m ceiling — must NOT receive.
    clients[1].voice.position = vec3(75.f, 0.f, 0.f);
    // Listener at 10 m is comfortably inside — must receive.
    clients[2].voice.position = vec3(10.f, 0.f, 0.f);

    auto bytes = fx.make_ingress(1, 0);
    fx.server.handle_voice_frame(clients[0].address, bytes.data(), bytes.size());

    ASSERT_EQ(fx.sent.size(), 1u);
    EXPECT_EQ(std::get<0>(fx.sent[0]), clients[2].address);
}

TEST(VoiceRelay, EndOfTalkNotRelayed) {
    RelayFixture fx;
    auto& clients = fx.server.get_clients_mutable();

    auto bytes = fx.make_ingress(1, voice_flags::END_OF_TALK);
    fx.server.handle_voice_frame(clients[0].address, bytes.data(), bytes.size());

    EXPECT_TRUE(fx.sent.empty());
}

TEST(VoiceRelay, ZeroLengthPayloadNotRelayed) {
    RelayFixture fx;
    auto& clients = fx.server.get_clients_mutable();

    auto bytes = fx.make_ingress(1, 0, /*fake=*/0, /*payload=*/{});
    fx.server.handle_voice_frame(clients[0].address, bytes.data(), bytes.size());

    EXPECT_TRUE(fx.sent.empty());
}

TEST(VoiceRelay, StaleReplayedSequenceDropped) {
    RelayFixture fx;
    auto& clients = fx.server.get_clients_mutable();

    // First frame at seq 100 — accepted, relayed.
    auto a = fx.make_ingress(100, 0);
    fx.server.handle_voice_frame(clients[0].address, a.data(), a.size());
    const size_t first_count = fx.sent.size();
    EXPECT_GT(first_count, 0u);

    // Replay of 100 — must be dropped by the server-side replay window guard.
    fx.server.handle_voice_frame(clients[0].address, a.data(), a.size());
    EXPECT_EQ(fx.sent.size(), first_count);
}

TEST(VoiceRelay, UnknownSenderDropped) {
    RelayFixture fx;

    // A packet from an address that matches no connected slot must be
    // dropped — prevents amplification via spoofed source IP.
    Address stranger{"198.51.100.5", 9999};
    auto bytes = fx.make_ingress(1, 0);
    fx.server.handle_voice_frame(stranger, bytes.data(), bytes.size());

    EXPECT_TRUE(fx.sent.empty());
}
