// scoring.glsl — Utility curves and normalization for behavior scoring
// Include via: #include "scoring.glsl"
#ifndef SCORING_GLSL
#define SCORING_GLSL

// Normalize value to 0..1 range
float normalize01(float value, float min_val, float max_val) {
    return clamp((value - min_val) / max(max_val - min_val, 0.001), 0.0, 1.0);
}

// Health score: 0 = dead, 1 = full health
float score_health(float health, float max_health) {
    return clamp(health / max(max_health, 0.001), 0.0, 1.0);
}

// Inverse health: 1 = dying, 0 = full health (for flee/heal behaviors)
float score_damage(float health, float max_health) {
    return 1.0 - score_health(health, max_health);
}

// Distance score: 1 = at position, 0 = at max_dist or beyond
float score_proximity(float distance, float max_dist) {
    return 1.0 - clamp(distance / max(max_dist, 0.001), 0.0, 1.0);
}

// Inverse distance: 0 = close, 1 = far
float score_distance(float distance, float max_dist) {
    return clamp(distance / max(max_dist, 0.001), 0.0, 1.0);
}

// Bell curve: peaks at center with given width (gaussian-ish)
float bell_curve(float x, float center, float width) {
    float d = (x - center) / max(width, 0.001);
    return exp(-0.5 * d * d);
}

// Smooth threshold: 0 below (threshold-edge), 1 above (threshold+edge)
float smooth_threshold(float value, float threshold, float edge) {
    return smoothstep(threshold - edge, threshold + edge, value);
}

// Ramp: linear increase from 0 at start to 1 at end
float ramp(float value, float start, float end) {
    return clamp((value - start) / max(end - start, 0.001), 0.0, 1.0);
}

// Weighted blend of two scores
float blend_scores(float a, float b, float t) {
    return mix(a, b, clamp(t, 0.0, 1.0));
}

// Max of multiple scores (pick strongest behavior)
float score_max3(float a, float b, float c) {
    return max(a, max(b, c));
}

float score_max4(float a, float b, float c, float d) {
    return max(max(a, b), max(c, d));
}

// Softmax-ish: bias toward highest score while keeping some weight on others
// temperature > 1 = more uniform, temperature < 1 = more winner-take-all
float biased_score(float score, float max_score, float temperature) {
    return pow(score / max(max_score, 0.001), 1.0 / max(temperature, 0.01));
}

// Cooldown check: returns 1.0 if cooldown has elapsed, 0.0 otherwise
float cooldown_ready(float cooldown_timer, float cooldown_duration) {
    return step(cooldown_duration, cooldown_timer);
}

#endif // SCORING_GLSL
