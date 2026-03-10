#pragma once
#include "core/result.h"
#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>

namespace odyssey::assets {

struct TextureData {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> pixels;
    std::filesystem::path source_path;
};

// Impure: load texture from image file (PNG, JPG, BMP, TGA)
Result<TextureData> load_texture(const std::filesystem::path& path);

// Pure: compute mip level count for given dimensions
uint32_t compute_mip_levels(uint32_t width, uint32_t height);

} // namespace odyssey::assets
