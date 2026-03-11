#pragma once

#include "core/types.h"

struct GLFWwindow;

namespace odyssey {

/// Manages GLFW keyboard and mouse input for FPS-style controls.
/// Uses static callbacks that forward to a module-level pointer (standard GLFW pattern).
class InputManager {
public:
    InputManager();
    ~InputManager();

    // Non-copyable, non-movable.
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    /// Register GLFW callbacks. Captures the cursor by default.
    void initialize(GLFWwindow* window);

    /// Call once per frame (after polling events) to finalize deltas and
    /// handle toggle logic such as ESC toggling cursor capture.
    void update();

    /// True while the given GLFW key code is held down.
    bool is_key_down(int glfw_key) const;

    /// Mouse movement since last update() call.
    vec2 mouse_delta() const { return mouse_delta_; }

    /// True while the given mouse button is held (0=left, 1=right, 2=middle).
    bool is_mouse_button_down(int button) const;

    /// True only on the first read after the button was pressed (consume-on-read).
    bool was_mouse_button_pressed(int button);

    /// Enable or disable cursor capture (hidden + grabbed for FPS mouselook).
    void set_cursor_captured(bool captured);

    /// Whether the cursor is currently captured.
    bool is_cursor_captured() const { return cursor_captured_; }

private:
    // GLFW static callbacks
    static void key_callback(GLFWwindow* window, int key, int scancode,
                             int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

    GLFWwindow* window_ = nullptr;

    // Key states — indexed by GLFW key code (up to GLFW_KEY_LAST + 1 = 349).
    static constexpr int MAX_KEYS = 512;
    bool keys_[MAX_KEYS] = {};

    // Mouse tracking
    vec2 mouse_delta_{0.0f, 0.0f};
    double last_mouse_x_ = 0.0;
    double last_mouse_y_ = 0.0;
    double accum_mouse_dx_ = 0.0;
    double accum_mouse_dy_ = 0.0;
    bool first_mouse_ = true;

    // Mouse buttons (up to 8)
    static constexpr int MAX_MOUSE_BUTTONS = 8;
    bool mouse_buttons_[MAX_MOUSE_BUTTONS] = {};
    bool mouse_just_pressed_[MAX_MOUSE_BUTTONS] = {};

    // Cursor capture
    bool cursor_captured_ = true;

    // ESC toggle tracking — only toggle on press, not hold
    bool esc_was_pressed_ = false;
};

} // namespace odyssey
