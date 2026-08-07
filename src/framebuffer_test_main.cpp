#include "framebuffer.hpp"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace
{
std::size_t passed = 0;
std::size_t failed = 0;


void check(
    bool condition,
    const char* test_name)
{
    if (condition)
    {
        ++passed;
        std::printf("[PASS] %s\n", test_name);
    }
    else
    {
        ++failed;
        std::printf("[FAIL] %s\n", test_name);
    }
}
}


int main()
{
    stdio_init_all();

    // Wait until the computer has opened the USB serial port.
    // 等待电脑打开 USB 串口，避免错过一次性测试结果。
    while (!stdio_usb_connected())
    {
        sleep_ms(100);
    }

    sleep_ms(200);

    std::printf(
        "\n=== MEDUSA FRAMEBUFFER TEST START ===\n");

    check(
        framebuffer::width == 256 &&
        framebuffer::height == 64 &&
        framebuffer::row_bytes == 128 &&
        framebuffer::size == 8192 &&
        framebuffer::data_size() == 8192,
        "Test 1: dimensions");

    framebuffer::clear(0x00);
    check(
        framebuffer::data()[0] == 0x00 &&
        framebuffer::data()[4096] == 0x00 &&
        framebuffer::data()[8191] == 0x00,
        "Test 2: clear black");

    framebuffer::clear(0x0A);
    check(
        framebuffer::data()[0] == 0xAA &&
        framebuffer::data()[4096] == 0xAA &&
        framebuffer::data()[8191] == 0xAA,
        "Test 3: clear grayscale A");

    framebuffer::clear(0xF4);
    check(
        framebuffer::data()[0] == 0x44,
        "Test 4: grayscale masking");

    framebuffer::clear(0x00);
    const bool first_even_pixel_set =
        framebuffer::set_pixel(0, 0, 0x0F);
    check(
        first_even_pixel_set &&
        framebuffer::data()[0] == 0xF0 &&
        framebuffer::get_pixel(0, 0) == 0x0F &&
        framebuffer::get_pixel(1, 0) == 0x00,
        "Test 5: first even pixel");

    const bool first_odd_pixel_set =
        framebuffer::set_pixel(1, 0, 0x03);
    check(
        first_odd_pixel_set &&
        framebuffer::data()[0] == 0xF3 &&
        framebuffer::get_pixel(0, 0) == 0x0F &&
        framebuffer::get_pixel(1, 0) == 0x03,
        "Test 6: first odd pixel");

    framebuffer::set_pixel(2, 0, 0x0C);
    check(
        framebuffer::data()[1] == 0xC0,
        "Test 7: second byte");

    framebuffer::clear(0x00);
    const bool last_pixel_set =
        framebuffer::set_pixel(255, 63, 0x07);
    check(
        last_pixel_set &&
        framebuffer::data()[8191] == 0x07 &&
        framebuffer::get_pixel(255, 63) == 0x07,
        "Test 8: last pixel");

    framebuffer::set_pixel(254, 63, 0x0B);
    check(
        framebuffer::data()[8191] == 0xB7,
        "Test 9: preceding pixel in last byte");

    const uint8_t before = framebuffer::data()[8191];
    const bool rejected_x =
        !framebuffer::set_pixel(256, 0, 15);
    const bool rejected_y =
        !framebuffer::set_pixel(0, 64, 15);
    const bool rejected_xy =
        !framebuffer::set_pixel(256, 64, 15);
    check(
        rejected_x &&
        rejected_y &&
        rejected_xy &&
        framebuffer::data()[8191] == before &&
        framebuffer::get_pixel(256, 0) == 0 &&
        framebuffer::get_pixel(0, 64) == 0,
        "Test 10: out-of-bounds coordinates");

    framebuffer::clear(0x00);

    constexpr uint8_t drawing_grayscale = 0x0F;

    for (std::size_t x = 0; x < framebuffer::width; ++x)
    {
        framebuffer::set_pixel(x, 0, drawing_grayscale);
        framebuffer::set_pixel(
            x,
            framebuffer::height - 1,
            drawing_grayscale);
    }

    for (std::size_t y = 0; y < framebuffer::height; ++y)
    {
        framebuffer::set_pixel(0, y, drawing_grayscale);
        framebuffer::set_pixel(
            framebuffer::width - 1,
            y,
            drawing_grayscale);
    }

    for (std::size_t x = 0; x < framebuffer::width; ++x)
    {
        const std::size_t y =
            x * (framebuffer::height - 1) /
            (framebuffer::width - 1);

        framebuffer::set_pixel(x, y, drawing_grayscale);
        framebuffer::set_pixel(
            framebuffer::width - 1 - x,
            y,
            drawing_grayscale);
    }

    constexpr std::size_t rectangle_width = 40;
    constexpr std::size_t rectangle_height = 20;
    constexpr std::size_t rectangle_x =
        (framebuffer::width - rectangle_width) / 2;
    constexpr std::size_t rectangle_y =
        (framebuffer::height - rectangle_height) / 2;

    for (std::size_t y = rectangle_y;
         y < rectangle_y + rectangle_height;
         ++y)
    {
        for (std::size_t x = rectangle_x;
             x < rectangle_x + rectangle_width;
             ++x)
        {
            framebuffer::set_pixel(x, y, drawing_grayscale);
        }
    }

    std::printf("----- FRAMEBUFFER ASCII PREVIEW -----\n");

    constexpr std::size_t preview_scale = 4;

    for (std::size_t block_y = 0;
         block_y < framebuffer::height / preview_scale;
         ++block_y)
    {
        for (std::size_t block_x = 0;
             block_x < framebuffer::width / preview_scale;
             ++block_x)
        {
            uint8_t maximum_grayscale = 0;

            for (std::size_t pixel_y = 0;
                 pixel_y < preview_scale;
                 ++pixel_y)
            {
                for (std::size_t pixel_x = 0;
                     pixel_x < preview_scale;
                     ++pixel_x)
                {
                    const uint8_t grayscale =
                        framebuffer::get_pixel(
                            block_x * preview_scale + pixel_x,
                            block_y * preview_scale + pixel_y);

                    if (grayscale > maximum_grayscale)
                    {
                        maximum_grayscale = grayscale;
                    }
                }
            }

            char preview_character = ' ';

            if (maximum_grayscale >= 11)
            {
                preview_character = '#';
            }
            else if (maximum_grayscale >= 6)
            {
                preview_character = '*';
            }
            else if (maximum_grayscale >= 1)
            {
                preview_character = '.';
            }

            std::putchar(preview_character);
        }

        std::putchar('\n');
    }

    std::printf("----- END PREVIEW -----\n");
    std::printf("Framebuffer tests complete\n");
    std::printf(
        "Passed: %u\n",
        static_cast<unsigned int>(passed));
    std::printf(
        "Failed: %u\n",
        static_cast<unsigned int>(failed));

    if (failed == 0)
    {
        std::printf("FRAMEBUFFER TEST RESULT: PASS\n");
    }
    else
    {
        std::printf("FRAMEBUFFER TEST RESULT: FAIL\n");
    }

    while (true)
    {
        sleep_ms(2000);
        std::printf("Framebuffer test firmware running\n");
    }
}
