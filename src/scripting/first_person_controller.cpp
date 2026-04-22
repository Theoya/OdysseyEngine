#include "scripting/first_person_controller.h"
#include "scripting/script_registry.h"
#include "app/input.h"
#include "app/game.h"
#include "core/types.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <spdlog/spdlog.h>

namespace odyssey::scripting {

ScriptResult FirstPersonController::pre_physics(ScriptContext& ctx) {
    // TODO: Integrate with actual InputManager once Game exposes input
    // For now, this is a stub that returns empty result.
    // The full implementation will:
    // 1. Read WASD + Space from input
    // 2. Read mouse delta for yaw/pitch
    // 3. Write velocity to entity's rigidbody (if present)
    // 4. Update yaw based on mouse input

    return ScriptResult();
}

ScriptResult FirstPersonController::post_physics(ScriptContext& ctx) {
    // TODO: Integrate with actual entity manager + camera system
    // For now, this is a stub that returns empty result.
    // The full implementation will:
    // 1. Read entity position from physics simulation
    // 2. Compute head-bob offset based on walking motion
    // 3. Write camera offset (position + head-bob) to render pipeline
    // 4. Fire footstep_event when grounded-transition detected (Phase 10+)

    return ScriptResult();
}

REGISTER_SCRIPT(FirstPersonController)

} // namespace odyssey::scripting
