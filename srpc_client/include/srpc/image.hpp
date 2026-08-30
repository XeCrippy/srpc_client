#pragma once

#include "srpc/types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace srpc {

enum class PixelFormat : std::uint8_t {
    rgba8,
    bgra8,
};

struct ImageView {
    std::span<const std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t stride = 0;
    PixelFormat format = PixelFormat::rgba8;
};

// Encodes an 8-bit RGBA PNG. The input may be RGBA or BGRA and may contain
// row padding through `stride`; padding bytes are not written to the image.
[[nodiscard]] ByteBuffer encode_png(const ImageView& image);

// Writes the same deterministic encoding to a caller-selected path. Parent
// directories are not created and no default or branded path is used.
void write_png(const std::filesystem::path& path, const ImageView& image);

} // namespace srpc
