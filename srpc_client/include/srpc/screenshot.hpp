#pragma once

#include "srpc/image.hpp"
#include "srpc/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace srpc {

inline constexpr std::size_t default_maximum_screenshot_size =
    64 * 1024 * 1024;

enum class ScreenshotUntileMode : std::uint8_t {
    xenos,
    // Kept for compatibility with captures that use the older Morton-style
    // fallback. Normal XBDM screenshots use the Xenos 2D layout above.
    morton,
};

// Values reported by Xbox 360 XBDM. These are packed Xenos descriptors, not
// the similarly named desktop Direct3D 9 enum values.
enum class ScreenshotFormat : std::uint32_t {
    a8r8g8b8 = 0x18280186,
    a2r10g10b10 = 0x182801B6,
};

struct ScreenshotMetadata {
    std::uint32_t pitch_bytes = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t framebuffer_size = 0;
    // Kept numeric so raw capture can preserve formats added by other XBDM
    // builds. decode_screenshot validates formats it knows how to convert.
    std::optional<std::uint32_t> format;
    std::uint32_t display_width = 0;
    std::uint32_t display_height = 0;
    std::uint32_t offset_x = 0;
    std::uint32_t offset_y = 0;
    std::optional<std::uint32_t> color_space;
};

struct RawScreenshot {
    ScreenshotMetadata metadata;
    ByteBuffer tiled_framebuffer;
};

struct ScreenshotImage {
    ScreenshotMetadata source;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    ByteBuffer bgra;

    [[nodiscard]] ImageView view() const noexcept {
        return {
            bgra,
            width,
            height,
            static_cast<std::size_t>(width) * 4,
            PixelFormat::bgra8};
    }
};

struct ScreenshotOptions {
    ScreenshotUntileMode untile_mode = ScreenshotUntileMode::xenos;
    bool compose_display_surface = false;
    // Display frontbuffers are normally XRGB rather than meaningful-alpha
    // images. Leave this false for opaque screenshots; raw capture always
    // preserves every source byte.
    bool preserve_alpha = false;
    // Empirical XBDM captures store packed 10-bit display words little-endian.
    // Big-endian remains available for unusual/custom monitor builds.
    Endian packed_10bit_endian = Endian::little;
    std::size_t maximum_framebuffer_size = default_maximum_screenshot_size;
    std::size_t maximum_decoded_size = default_maximum_screenshot_size;
};

// Converts the captured tiled framebuffer into tightly packed BGRA pixels.
// Known Xbox format descriptors are channel-converted; captures from older
// XBDM builds that omit `format` retain the established BGRA-byte assumption.
// When composition is enabled, XBDM's display dimensions and offsets are
// applied. Scaling uses nearest-neighbour sampling, matching the console's
// integer-pixel screenshot behavior.
[[nodiscard]] ScreenshotImage decode_screenshot(
    const RawScreenshot& screenshot,
    ScreenshotOptions options = {});

} // namespace srpc
