#define STB_IMAGE_IMPLEMENTATION
#include "assets/texture_loader.h"
#include <stb_image.h>
#include <spdlog/spdlog.h>
#include <cmath>

namespace odyssey::assets {

Result<TextureData> load_texture(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<TextureData>::err("Texture file not found: " + path.string());
    }

    std::string path_str = path.string();
    int width = 0, height = 0, channels = 0;

    // Request 4 channels (RGBA) for consistency
    stbi_uc* data = stbi_load(path_str.c_str(), &width, &height, &channels, 4);

    if (!data) {
        const char* reason = stbi_failure_reason();
        return Result<TextureData>::err(
            "Failed to load texture '" + path_str + "': "
            + (reason ? reason : "unknown error"));
    }

    TextureData texture;
    texture.width = width;
    texture.height = height;
    texture.channels = 4; // We requested RGBA
    texture.source_path = path;

    // Copy pixel data to vector
    size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    texture.pixels.assign(data, data + pixel_count);

    stbi_image_free(data);

    spdlog::info("Loaded texture '{}': {}x{} ({} channels)",
                 path.filename().string(), width, height, channels);
    return Result<TextureData>::ok(std::move(texture));
}

uint32_t compute_mip_levels(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return 1;
    uint32_t max_dim = (width > height) ? width : height;
    return static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(max_dim)))) + 1;
}

} // namespace odyssey::assets
