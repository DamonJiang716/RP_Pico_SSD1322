#pragma once

#include "sprite.hpp"

#include <cstddef>
#include <cstdint>

namespace graphics
{
enum class Rotation : uint8_t
{
    Degrees0,
    Degrees90,
    Degrees180,
    Degrees270
};

bool draw_horizontal_line(
    std::int32_t x,
    std::int32_t y,
    std::int32_t length,
    uint8_t grayscale);

bool draw_vertical_line(
    std::int32_t x,
    std::int32_t y,
    std::int32_t length,
    uint8_t grayscale);

bool draw_line(
    std::int32_t x0,
    std::int32_t y0,
    std::int32_t x1,
    std::int32_t y1,
    uint8_t grayscale);

bool draw_rectangle(
    std::int32_t x,
    std::int32_t y,
    std::int32_t width,
    std::int32_t height,
    uint8_t grayscale);

bool fill_rectangle(
    std::int32_t x,
    std::int32_t y,
    std::int32_t width,
    std::int32_t height,
    uint8_t grayscale);

bool draw_char(
    std::int32_t x,
    std::int32_t y,
    char character,
    uint8_t foreground_grayscale,
    uint8_t background_grayscale);

bool draw_text(
    std::int32_t x,
    std::int32_t y,
    const char* text,
    uint8_t foreground_grayscale,
    uint8_t background_grayscale);

bool draw_bitmap_4bpp(
    std::int32_t x,
    std::int32_t y,
    const uint8_t* bitmap,
    std::size_t bitmap_size,
    std::size_t bitmap_width,
    std::size_t bitmap_height,
    Rotation rotation);

bool draw_sprite_2bpp(
    std::int32_t x,
    std::int32_t y,
    const sprite::Sprite2bpp& image,
    Rotation rotation);
}
