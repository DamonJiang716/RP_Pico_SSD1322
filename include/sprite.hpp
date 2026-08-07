#pragma once

#include <cstddef>
#include <cstdint>

namespace sprite
{
struct Sprite2bpp
{
    const std::uint8_t* data;
    std::size_t data_size;
    std::size_t width;
    std::size_t height;
};

inline constexpr std::size_t bits_per_pixel = 2;
inline constexpr std::size_t pixels_per_byte = 4;
}
