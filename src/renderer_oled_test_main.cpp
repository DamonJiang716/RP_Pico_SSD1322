#include "framebuffer.hpp"
#include "graphics.hpp"
#include "ssd1322.hpp"

#include "pico/stdlib.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace
{
static_assert(
    framebuffer::width ==
    ssd1322::display_width);

static_assert(
    framebuffer::height ==
    ssd1322::display_height);

static_assert(
    framebuffer::size ==
    ssd1322::framebuffer_size);

static constexpr uint8_t arrow_bitmap_8x8[] =
{
    0x00, 0x00, 0xF0, 0x00,
    0x00, 0x00, 0xFF, 0x00,
    0xFF, 0xFF, 0xFF, 0xF0,
    0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xF0,
    0x00, 0x00, 0xFF, 0x00,
    0x00, 0x00, 0xF0, 0x00
};

static_assert(sizeof(arrow_bitmap_8x8) == 32);


[[noreturn]]
void halt_with_message(const char* message)
{
    std::printf("%s\n", message);

    while (true)
    {
        tight_loop_contents();
    }
}


bool draw_renderer_oled_test_frame()
{
    framebuffer::clear(0x00);

    if (!graphics::draw_rectangle(
            0,
            0,
            static_cast<std::int32_t>(framebuffer::width),
            static_cast<std::int32_t>(framebuffer::height),
            0x0F))
    {
        return false;
    }

    if (!graphics::draw_line(
            0,
            0,
            255,
            63,
            0x05))
    {
        return false;
    }

    if (!graphics::draw_line(
            0,
            63,
            255,
            0,
            0x09))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            76,
            25,
            104,
            14,
            0x00))
    {
        return false;
    }

    if (!graphics::draw_text(
            92,
            28,
            "RENDERER",
            0x0F,
            0x00))
    {
        return false;
    }

    if (!graphics::draw_bitmap_4bpp(
            16,
            8,
            arrow_bitmap_8x8,
            sizeof(arrow_bitmap_8x8),
            8,
            8,
            graphics::Rotation::Degrees0))
    {
        return false;
    }

    if (!graphics::draw_bitmap_4bpp(
            232,
            8,
            arrow_bitmap_8x8,
            sizeof(arrow_bitmap_8x8),
            8,
            8,
            graphics::Rotation::Degrees90))
    {
        return false;
    }

    if (!graphics::draw_bitmap_4bpp(
            232,
            48,
            arrow_bitmap_8x8,
            sizeof(arrow_bitmap_8x8),
            8,
            8,
            graphics::Rotation::Degrees180))
    {
        return false;
    }

    if (!graphics::draw_bitmap_4bpp(
            16,
            48,
            arrow_bitmap_8x8,
            sizeof(arrow_bitmap_8x8),
            8,
            8,
            graphics::Rotation::Degrees270))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            96, 44, 12, 10, 0x03))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            112, 44, 12, 10, 0x06))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            128, 44, 12, 10, 0x09))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            144, 44, 12, 10, 0x0C))
    {
        return false;
    }

    return true;
}
}


int main()
{
    stdio_init_all();

    ssd1322::initialise_hardware();

    sleep_ms(1000);

    std::printf(
        "Renderer OLED integration test starting\n");

    if (!ssd1322::initialise_display())
    {
        halt_with_message(
            "SSD1322 initialisation failed");
    }

    if (!draw_renderer_oled_test_frame())
    {
        halt_with_message(
            "Renderer OLED frame drawing failed");
    }

    if (!ssd1322::write_full_frame(
            framebuffer::data(),
            framebuffer::data_size()))
    {
        halt_with_message(
            "Renderer OLED frame transfer failed");
    }

    std::printf(
        "Renderer OLED frame sent\n");

    while (true)
    {
        sleep_ms(2000);

        std::printf(
            "Renderer OLED test firmware running\n");
    }
}
