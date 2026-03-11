#include "app/input.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <cstring>

namespace odyssey {

// Module-level pointer so static GLFW callbacks can reach the InputManager.
static InputManager* g_input = nullptr;

InputManager::InputManager() {
    std::memset(keys_, 0, sizeof(keys_));
    std::memset(mouse_buttons_, 0, sizeof(mouse_buttons_));
    std::memset(mouse_just_pressed_, 0, sizeof(mouse_just_pressed_));
}

InputManager::~InputManager() {
    if (g_input == this) {
        g_input = nullptr;
    }
}

void InputManager::initialize(GLFWwindow* window) {
    window_ = window;
    g_input = this;

    glfwSetKeyCallback(window_, key_callback);
    glfwSetCursorPosCallback(window_, cursor_pos_callback);
    glfwSetMouseButtonCallback(window_, mouse_button_callback);

    // Start with cursor captured for FPS mouselook.
    set_cursor_captured(true);

    // Seed the initial mouse position so the first frame doesn't produce a
    // large delta from (0,0) to the actual cursor position.
    glfwGetCursorPos(window_, &last_mouse_x_, &last_mouse_y_);
    first_mouse_ = true;

    spdlog::info("InputManager initialized");
}

void InputManager::update() {
    // Finalize mouse delta for this frame, then reset accumulator.
    mouse_delta_ = vec2(static_cast<float>(accum_mouse_dx_),
                        static_cast<float>(accum_mouse_dy_));
    accum_mouse_dx_ = 0.0;
    accum_mouse_dy_ = 0.0;

    // ESC toggles cursor capture (edge-triggered: only on key-down transition).
    bool esc_down = is_key_down(GLFW_KEY_ESCAPE);
    if (esc_down && !esc_was_pressed_) {
        set_cursor_captured(!cursor_captured_);
    }
    esc_was_pressed_ = esc_down;
}

bool InputManager::is_key_down(int glfw_key) const {
    if (glfw_key < 0 || glfw_key >= MAX_KEYS) return false;
    return keys_[glfw_key];
}

bool InputManager::is_mouse_button_down(int button) const {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return mouse_buttons_[button];
}

bool InputManager::was_mouse_button_pressed(int button) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    bool pressed = mouse_just_pressed_[button];
    mouse_just_pressed_[button] = false;
    return pressed;
}

void InputManager::set_cursor_captured(bool captured) {
    cursor_captured_ = captured;
    if (window_) {
        glfwSetInputMode(window_, GLFW_CURSOR,
                         captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
    // Reset first-mouse flag so the next cursor event doesn't cause a jump.
    first_mouse_ = true;

    spdlog::debug("Cursor capture: {}", captured ? "on" : "off");
}

// ---------------------------------------------------------------------------
// Static GLFW callbacks
// ---------------------------------------------------------------------------

void InputManager::key_callback(GLFWwindow* /*window*/, int key,
                                 int /*scancode*/, int action, int /*mods*/) {
    if (!g_input) return;
    if (key < 0 || key >= MAX_KEYS) return;

    if (action == GLFW_PRESS) {
        g_input->keys_[key] = true;
    } else if (action == GLFW_RELEASE) {
        g_input->keys_[key] = false;
    }
    // GLFW_REPEAT is ignored — we only care about down/up.
}

void InputManager::cursor_pos_callback(GLFWwindow* /*window*/,
                                        double xpos, double ypos) {
    if (!g_input) return;

    if (g_input->first_mouse_) {
        g_input->last_mouse_x_ = xpos;
        g_input->last_mouse_y_ = ypos;
        g_input->first_mouse_ = false;
        return;
    }

    double dx = xpos - g_input->last_mouse_x_;
    double dy = ypos - g_input->last_mouse_y_;
    g_input->last_mouse_x_ = xpos;
    g_input->last_mouse_y_ = ypos;

    // Only accumulate when cursor is captured (FPS mouselook mode).
    if (g_input->cursor_captured_) {
        g_input->accum_mouse_dx_ += dx;
        g_input->accum_mouse_dy_ += dy;
    }
}

void InputManager::mouse_button_callback(GLFWwindow* /*window*/, int button,
                                          int action, int /*mods*/) {
    if (!g_input) return;
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) return;

    if (action == GLFW_PRESS) {
        g_input->mouse_buttons_[button] = true;
        g_input->mouse_just_pressed_[button] = true;
    } else if (action == GLFW_RELEASE) {
        g_input->mouse_buttons_[button] = false;
    }
}

} // namespace odyssey
