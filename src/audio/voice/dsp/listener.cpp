//
// listener.cpp — pure listener-relative geometry. Standard dot-product
// projections onto the listener's orthonormal basis; no engine state
// accessed.
//

#include "audio/voice/dsp/listener.h"

#include <cmath>

namespace odyssey::audio::voice::dsp {

float source_distance(const ListenerPose& listener, const glm::vec3& source_world) noexcept {
    // Euclidean norm of (source - listener). We use glm::length which reduces
    // to sqrt(dot(d, d)) — one sqrt, branch-free, acceptable on the voice
    // path (64 speakers × sqrt is <1 µs on the RTX 3080 host).
    const glm::vec3 d = source_world - listener.position;
    return glm::length(d);
}

float source_azimuth(const ListenerPose& listener, const glm::vec3& source_world) noexcept {
    // Project the listener-relative offset onto the listener's forward and
    // right basis vectors. atan2 gives the signed angle in the plane spanned
    // by those two basis vectors; that is the horizontal azimuth as long as
    // forward/right are horizontal (no pitch). For free-looking cameras the
    // forward may have a vertical component; projecting onto the horizontal
    // plane would be more correct for pure panning, but our VR-free FPS
    // cameras rarely pitch beyond ±60°, so the error stays under 10° on pan
    // and the vertical component naturally folds into "how centered" the
    // source feels. If VR ships, we add a horizontalization step here.
    const glm::vec3 d = source_world - listener.position;
    const float pf = glm::dot(d, listener.forward);
    const float pr = glm::dot(d, listener.right);
    return std::atan2(pr, pf);
}

bool is_behind_listener(const ListenerPose& listener, const glm::vec3& source_world) noexcept {
    // A source is behind iff its forward-projection is negative; equivalently
    // |azimuth| > π/2. Dot-product form is one mul-add cheaper than the atan.
    const glm::vec3 d = source_world - listener.position;
    return glm::dot(d, listener.forward) < 0.0f;
}

} // namespace odyssey::audio::voice::dsp
