#include "framebuffer.hpp"
#include "graphics.hpp"
#include "ssd1322.hpp"

#include "pico/stdlib.h"

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

[[noreturn]]
void halt_with_message(const char* message)
{
    std::printf("%s\n", message);

    while (true)
    {
        tight_loop_contents();
    }
}

bool draw_integrated_test_frame()
{
    framebuffer::clear(0x00);

    // Full-screen outer border.
    if (!graphics::draw_rectangle(
            0,
            0,
            static_cast<std::int32_t>(
                framebuffer::width),
            static_cast<std::int32_t>(
                framebuffer::height),
            0x0F))
    {
        return false;
    }

    // Gray title bar.
    if (!graphics::fill_rectangle(
            4,
            3,
            248,
            12,
            0x03))
    {
        return false;
    }

    // Title. Background matches title bar.
    if (!graphics::draw_text(
            8,
            5,
            "MEDUSA",
            0x0F,
            0x03))
    {
        return false;
    }

    // Uppercase letters and digits.
    if (!graphics::draw_text(
            8,
            20,
            "HELLO 123",
            0x0C,
            0x00))
    {
        return false;
    }

    // Uppercase, lowercase and digits.
    if (!graphics::draw_text(
            8,
            32,
            "ABC xyz 789",
            0x0F,
            0x00))
    {
        return false;
    }

    // Five grayscale blocks.
    if (!graphics::fill_rectangle(
            8, 48, 40, 10, 0x03))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            56, 48, 40, 10, 0x06))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            104, 48, 40, 10, 0x09))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            152, 48, 40, 10, 0x0C))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            200, 48, 40, 10, 0x0F))
    {
        return false;
    }

    return true;
}
}

int main()
{
    stdio_init_all();

    // Configure SPI and place OLED control pins
    // into known states immediately.
    ssd1322::initialise_hardware();

    // Allow the USB serial port time to enumerate.
    sleep_ms(1000);

    std::printf(
        "Medusa integrated OLED test starting\n");

    if (!ssd1322::initialise_display())
    {
        halt_with_message(
            "SSD1322 initialisation sequence failed");
    }

    std::printf(
        "SSD1322 initialisation sequence sent\n");

    if (!draw_integrated_test_frame())
    {
        halt_with_message(
            "Integrated frame drawing failed");
    }

    if (!ssd1322::write_full_frame(
            framebuffer::data(),
            framebuffer::data_size()))
    {
        halt_with_message(
            "Integrated display frame write failed");
    }

    std::printf(
        "Integrated text and graphics frame sent\n");

    while (true)
    {
        std::printf(
            "Medusa display firmware running\n");

        sleep_ms(2000);
    }
}
