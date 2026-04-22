// music_director.cpp — MusicDirector subsystem implementation.
//
// Pure-core design: MusicDirector does not spawn threads, does not own WASAPI,
// does not hold locks. It publishes an immutable MixPlan snapshot per tick via
// atomic pointer swap (double-buffer). WASAPI callback reads the latest snapshot.
//

#include "audio/music/music_director.h"
#include "audio/music/detail/bar_clock.h"
#include "audio/music/detail/sidechain_ducker.h"
#include "audio/music/detail/leitmotif_table.h"

#include <atomic>
#include <memory>

namespace odyssey::audio::music {

using detail::BarClock;
using detail::SidechainDuckerState;
using detail::SidechainDuckerParams;
using detail::LeitmotifTable;

// Hidden implementation details.
namespace {

struct MusicDirectorImpl {
    BarClock bar_clock{120.0f, 48000};
    LeitmotifTable leitmotif_table;
    SidechainDuckerState ducker_state;
    SidechainDuckerParams ducker_params;
    uint32_t current_theme_id = 0;
    std::vector<float> current_intensities;
    std::string current_mood_tag_val;
};

// Global instance (stub for now; future: per-engine instance).
static MusicDirectorImpl g_music_director;

} // namespace

Result<Unit, MusicError>
set_scene_theme(uint32_t theme_id, TransitionMode mode) {
    // TODO: Validate theme exists, apply quantization, transition state.
    g_music_director.current_theme_id = theme_id;
    return Result<Unit, MusicError>::ok(Unit{});
}

Result<Unit, MusicError>
set_intensity_layers(const std::vector<float>& intensity_targets) {
    // TODO: Validate ranges, update envelope targets.
    for (float t : intensity_targets) {
        if (t < 0.0f || t > 1.0f) {
            return Result<Unit, MusicError>::err(MusicError::InvalidIntensityRange);
        }
    }
    g_music_director.current_intensities = intensity_targets;
    return Result<Unit, MusicError>::ok(Unit{});
}

Result<Unit, MusicError>
fire_stinger(uint32_t stinger_id, StingerIntent intent, TransitionMode sync) {
    // TODO: Queue stinger, validate ID, apply quantization.
    return Result<Unit, MusicError>::ok(Unit{});
}

void tick(float dt_seconds) {
    // Advance bar_clock.
    g_music_director.bar_clock.tick(dt_seconds);

    // Tick ducker (voice-centric sidechain).
    bool any_voice = false;  // TODO: Query from voice_bus
    bool stinger_live = false;  // TODO: Query stinger queue
    tick_sidechain_ducker(g_music_director.ducker_state, any_voice, stinger_live,
                          dt_seconds, g_music_director.ducker_params);

    // TODO: Publish MixPlan snapshot via atomic swap.
}

MusicDirectorSnapshot debug_snapshot() {
    MusicDirectorSnapshot snap;
    snap.current_bar = g_music_director.bar_clock.bar();
    snap.current_beat = g_music_director.bar_clock.beat();
    snap.bar_phase = g_music_director.bar_clock.phase();
    snap.active_theme_id = g_music_director.current_theme_id;
    snap.intensity_layers = g_music_director.current_intensities;
    snap.current_mood_tag = g_music_director.current_mood_tag_val;
    return snap;
}

std::string current_mood_tag() {
    return g_music_director.current_mood_tag_val;
}

} // namespace odyssey::audio::music
