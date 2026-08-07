#pragma once

#include "sprite.hpp"

#include <cstddef>
#include <cstdint>

// Source image: test_arrow.png
// Dimensions: 5 x 2
// Format: 2bpp grayscale, row-major, 4 pixels per byte.
// Each row starts at a new byte; unused low bits are zero.
// Data bytes: 4

namespace generated_assets
{

inline constexpr std::uint8_t test_5x2_data[] =
{
    0xFF, 0xC0, 0xFF, 0xC0,
};

inline constexpr sprite::Sprite2bpp test_5x2 =
{
    test_5x2_data,
    sizeof(test_5x2_data),
    5,
    2
};

}
