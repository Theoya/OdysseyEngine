// debug.glsl — Debug output helpers for behavior visualization

// Write a color to debug buffer for visualization
void debug_color(uint idx, vec3 color) {
    debug_data[idx] = vec4(color, 1.0);
}

// Debug: show current state as color
void debug_state_color(uint idx, uint state) {
    vec3 color = vec3(0.5); // gray = unknown
    if (state == 0u) color = vec3(0.0, 1.0, 0.0);  // green = idle
    if (state == 1u) color = vec3(0.0, 0.0, 1.0);   // blue = patrol
    if (state == 2u) color = vec3(1.0, 1.0, 0.0);   // yellow = alert
    if (state == 3u) color = vec3(1.0, 0.0, 0.0);   // red = combat
    if (state == 4u) color = vec3(1.0, 0.5, 0.0);   // orange = flee
    if (state == 5u) color = vec3(0.2, 0.2, 0.2);   // dark gray = dead
    debug_data[idx] = vec4(color, 1.0);
}

// Debug: encode a float value as grayscale
void debug_value(uint idx, float value) {
    debug_data[idx] = vec4(vec3(clamp(value, 0.0, 1.0)), 1.0);
}

// Debug: encode two scores as red/green channels
void debug_two_scores(uint idx, float score_a, float score_b) {
    debug_data[idx] = vec4(score_a, score_b, 0.0, 1.0);
}

// Debug: encode a direction as color (normalized xyz mapped to rgb)
void debug_direction(uint idx, vec3 dir) {
    debug_data[idx] = vec4(dir * 0.5 + 0.5, 1.0);
}

// Debug: pack 4 values
void debug_pack4(uint idx, float a, float b, float c, float d) {
    debug_data[idx] = vec4(a, b, c, d);
}
