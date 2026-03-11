#pragma once

#include "app/game.h"

class MyGame : public odyssey::Game {
public:
    odyssey::Result<bool> on_init(odyssey::GameContext& ctx) override;
    void on_tick(odyssey::GameContext& ctx) override;
    const std::vector<odyssey::RenderEntity>& get_renderables() const override;
    odyssey::HUDParams get_hud_params() const override;
    void on_shutdown() override;

private:
    std::vector<odyssey::RenderEntity> renderables_;
};
