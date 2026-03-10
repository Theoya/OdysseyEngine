#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include "core/types.h"

int main(int argc, char* argv[]) {
    CLI::App app{"OdysseyEngine — GPU-maximalist 3D engine"};

    std::string config_path = "engine.xml";
    app.add_option("-c,--config", config_path, "Path to engine.xml configuration file");

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Enable verbose logging");

    CLI11_PARSE(app, argc, argv);

    if (verbose) {
        spdlog::set_level(spdlog::level::debug);
    }

    spdlog::info("OdysseyEngine v{}.{}.{}", 0, 1, 0);
    spdlog::info("Config: {}", config_path);

    // Engine initialization will go here in future phases.

    return 0;
}
