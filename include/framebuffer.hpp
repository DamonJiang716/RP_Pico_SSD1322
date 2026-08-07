#pragma once

#include <cstddef>
#include <cstdint>

namespace framebuffer
{
inline constexpr std::size_t width = 256;
inline constexpr std::size_t height = 64;
inline constexpr std::size_t pixels_per_byte = 2;

inline constexpr std::size_t row_bytes =
    width / pixels_per_byte;

inline constexpr std::size_t size =
    width * height / pixels_per_byte;

static_assert(width == 256);
static_assert(height == 64);
static_assert(row_bytes == 128);
static_assert(size == 8192);

void clear(uint8_t grayscale);

bool set_pixel(
    std::size_t x,
    std::size_t y,
    uint8_t grayscale);

uint8_t get_pixel(
    std::size_t x,
    std::size_t y);

const uint8_t* data();

std::size_t data_size();
}
