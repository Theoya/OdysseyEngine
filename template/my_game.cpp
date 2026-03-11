#include "my_game.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

odyssey::Result<bool> MyGame::on_init(odyssey::GameContext& ctx) {
    spdlog::info("MyGame initialized");

    // Ground plane
    renderables_.push_back({
        {0.f, 0.f, 0.f},           // position
        {0.3f, 0.5f, 0.3f, 1.0f},  // color (green-gray)
        1.0f,                        // scale
        2                            // mesh_type: GROUND_PLANE
    });

    // A spinning box
    renderables_.push_back({
        {0.f, 1.f, 0.f},
        {0.8f, 0.2f, 0.2f, 1.0f},
        1.0f,
        0  // mesh_type: BOX
    });

    return odyssey::Result<bool>::ok(true);
}

void MyGame::on_tick(odyssey::GameContext& ctx) {
    // Spin the box
    if (renderables_.size() > 1) {
        float angle = ctx.total_time * 1.0f;
        renderables_[1].position.x = std::sin(angle) * 3.0f;
        renderables_[1].position.z = std::cos(angle) * 3.0f;
    }
}

const std::vector<odyssey::RenderEntity>& MyGame::get_renderables() const {
    return renderables_;
}

odyssey::HUDParams MyGame::get_hud_params() const {
    return {};
}

void MyGame::on_shutdown() {
    spdlog::info("MyGame shut down");
}

// Factory function — the engine calls this to create the game.
std::unique_ptr<odyssey::Game> odyssey::create_game() {
    return std::make_unique<MyGame>();
}
