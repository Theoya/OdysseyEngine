#include "audio/voice/voice_frame.h"

#include <cstring>

namespace odyssey::audio::voice {

using namespace odyssey::net;

std::string to_string(ProtocolError e) {
    switch (e) {
        case ProtocolError::TruncatedHeader:    return "TruncatedHeader";
        case ProtocolError::BadProtocolId:      return "BadProtocolId";
        case ProtocolError::BadProtocolVersion: return "BadProtocolVersion";
        case ProtocolError::WrongPacketType:    return "WrongPacketType";
        case ProtocolError::TruncatedSubHeader: return "TruncatedSubHeader";
        case ProtocolError::OversizePayload:    return "OversizePayload";
        case ProtocolError::ReservedFlagsSet:   return "ReservedFlagsSet";
    }
    return "Unknown";
}

std::vector<uint8_t> serialize_voice_frame(const VoiceFrame& vf) {
    PacketWriter w;
    w.write_header(vf.header);
    write_voice_subheader(w, vf.voice);
    if (!vf.opus_payload.empty()) {
        w.write_bytes(vf.opus_payload.data(), vf.opus_payload.size());
    }
    return std::vector<uint8_t>(w.data(), w.data() + w.size());
}

Result<VoiceFrame, ProtocolError>
deserialize_voice_frame(const uint8_t* data, size_t size) {
    using R = Result<VoiceFrame, ProtocolError>;

    // Header size = 16 (see PacketHeader: u32+u16+u16+u32+u8+u8[3]).
    constexpr size_t kHeaderSize = 16;
    constexpr size_t kSubHeaderSize = 8;

    if (size < kHeaderSize) {
        return R::err(ProtocolError::TruncatedHeader);
    }
    if (size > MAX_PACKET_SIZE) {
        // Enforce the MTU ceiling here. A malicious peer that sends a 3 KB
        // frame would otherwise coerce the relay into forwarding something
        // that will IP-fragment and degrade the whole network path.
        return R::err(ProtocolError::OversizePayload);
    }

    PacketReader r(data, size);
    PacketHeader hdr = r.read_header();

    if (hdr.protocol_id != PROTOCOL_ID) {
        return R::err(ProtocolError::BadProtocolId);
    }
    if (hdr.type != PacketType::VOICE_FRAME) {
        return R::err(ProtocolError::WrongPacketType);
    }
    if (size < kHeaderSize + kSubHeaderSize) {
        return R::err(ProtocolError::TruncatedSubHeader);
    }

    VoiceSubHeader sh = read_voice_subheader(r);

    // Reject any reserved flag bits. This is a forward-compatibility lock:
    // if a v3 client sends a frame with new flag bits to a v2 server, we'd
    // rather drop than silently mis-interpret. Design doc §5 pins this.
    if ((sh.flags & voice_flags::RESERVED_MASK) != 0) {
        return R::err(ProtocolError::ReservedFlagsSet);
    }

    VoiceFrame vf;
    vf.header = hdr;
    vf.voice = sh;

    const size_t payload_len = size - (kHeaderSize + kSubHeaderSize);
    if (payload_len > 0) {
        vf.opus_payload.resize(payload_len);
        r.read_bytes(vf.opus_payload.data(), payload_len);
    }
    return R::ok(std::move(vf));
}

} // namespace odyssey::audio::voice
