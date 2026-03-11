#include "cli/cli.h"
#include "app/engine.h"
#include "app/game.h"

#include <spdlog/spdlog.h>

#include <iostream>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    // --- CLI ---
    auto cli_result = odyssey::cli::run_cli(argc, argv);

    if (!cli_result.output.empty()) {
        std::cout << cli_result.output << std::endl;
    }

    // If the CLI already resolved (build, test, nadir, scene) just exit.
    if (!cli_result.should_run_engine) {
        return cli_result.exit_code;
    }

    // --- Engine ---
    // Parse the engine config file.
    auto config_result = odyssey::parse_engine_config("engine.xml");
    if (config_result.is_err()) {
        spdlog::error("Failed to parse engine config: {}", config_result.error());
        return 1;
    }

    auto config = config_result.value();

    // Apply any CLI overrides.
    if (!cli_result.scene_path.empty()) {
        spdlog::info("Scene override from CLI: {}", cli_result.scene_path);
    }

    // Create the game (factory function defined by the game, not the engine).
    auto game = odyssey::create_game();

    // Boot the engine with the game.
    odyssey::Engine engine;
    auto init_result = engine.initialize(config, std::move(game));
    if (init_result.is_err()) {
        spdlog::error("Engine initialization failed: {}", init_result.error());
        return 1;
    }

    engine.run();
    engine.shutdown();

    return 0;
}
