#pragma once

namespace odyssey::editor {

struct EditorState;

// Pure helper: compute exponential moving average.
// EMA = α * new_value + (1 - α) * prev_value
// prev: the previous EMA value
// new_val: the new sample value
// alpha: smoothing factor [0, 1]
// Returns: updated EMA
float compute_fps_ema(float prev, float new_val, float alpha);

// Draw the editor status bar: pinned to bottom of viewport.
// Shows scene path, entity count, selection, FPS, GPU mem, and mode pill.
void draw_status_bar(EditorState& state, float fps, float dt_ms);

}  // namespace odyssey::editor
