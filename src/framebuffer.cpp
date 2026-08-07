#include "framebuffer.hpp"

#include <array>

namespace
{
std::array<uint8_t, framebuffer::size> pixels{};
}

namespace framebuffer
{
void clear(uint8_t grayscale)
{
    grayscale &= 0x0F;

    const uint8_t packed =
        static_cast<uint8_t>(
            (grayscale << 4) | grayscale);

    pixels.fill(packed);
}


bool set_pixel(
    std::size_t x,
    std::size_t y,
    uint8_t grayscale)
{
    if (x >= width || y >= height)
    {
        return false;
    }

    grayscale &= 0x0F;

    const std::size_t byte_index =
        y * row_bytes +
        x / 2;

    if ((x % 2) == 0)
    {
        pixels[byte_index] =
            static_cast<uint8_t>(
                (pixels[byte_index] & 0x0F) |
                (grayscale << 4));
    }
    else
    {
        pixels[byte_index] =
            static_cast<uint8_t>(
                (pixels[byte_index] & 0xF0) |
                grayscale);
    }

    return true;
}


uint8_t get_pixel(
    std::size_t x,
    std::size_t y)
{
    if (x >= width || y >= height)
    {
        return 0;
    }

    const std::size_t byte_index =
        y * row_bytes +
        x / 2;

    if ((x % 2) == 0)
    {
        return static_cast<uint8_t>(
            pixels[byte_index] >> 4);
    }

    return static_cast<uint8_t>(
        pixels[byte_index] & 0x0F);
}


const uint8_t* data()
{
    return pixels.data();
}


std::size_t data_size()
{
    return pixels.size();
}
}
