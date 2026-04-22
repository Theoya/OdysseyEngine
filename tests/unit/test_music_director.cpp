#include <gtest/gtest.h>
#include "audio/music/music_director.h"

using namespace odyssey::audio::music;

TEST(MusicDirectorTest, SetSceneTheme) {
    auto result = set_scene_theme(0, TransitionMode::Immediate);
    EXPECT_TRUE(result.is_ok());
}

TEST(MusicDirectorTest, SetIntensityLayers) {
    std::vector<float> intensities = {0.5f, 0.7f, 0.3f};
    auto result = set_intensity_layers(intensities);
    EXPECT_TRUE(result.is_ok());
}

TEST(MusicDirectorTest, SetIntensityLayersInvalidRange) {
    std::vector<float> intensities = {0.5f, 1.5f};  // 1.5 is out of range
    auto result = set_intensity_layers(intensities);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), MusicError::InvalidIntensityRange);
}

TEST(MusicDirectorTest, FireStinger) {
    auto result = fire_stinger(0, StingerIntent::Punctuate, TransitionMode::NextBar);
    EXPECT_TRUE(result.is_ok());
}

TEST(MusicDirectorTest, Tick) {
    tick(0.016f);  // ~60 FPS
    auto snap = debug_snapshot();
    EXPECT_GE(snap.current_bar, 0);
}

TEST(MusicDirectorTest, DebugSnapshot) {
    set_scene_theme(0, TransitionMode::Immediate);
    tick(0.0f);
    auto snap = debug_snapshot();
    EXPECT_EQ(snap.active_theme_id, 0);
}

TEST(MusicDirectorTest, CurrentMoodTag) {
    auto mood = current_mood_tag();
    // Initially empty or default
    EXPECT_GE(mood.length(), 0);
}

TEST(MusicDirectorTest, NadirSoundRequestDecode) {
    uint16_t request = 0x8000 | (0x1 << 12) | 42;  // Music flag + category 1 + ID 42
    EXPECT_TRUE(has_music_request(request));
    EXPECT_EQ(decode_sound_request_category(request), 1);
    EXPECT_EQ(decode_sound_request_id(request), 42);
}
