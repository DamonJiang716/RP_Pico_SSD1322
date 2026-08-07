#include "framebuffer.hpp"
#include "graphics.hpp"

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


bool row_equals(
    std::size_t x_begin,
    std::size_t x_end,
    std::size_t y,
    uint8_t grayscale)
{
    for (std::size_t x = x_begin; x < x_end; ++x)
    {
        if (framebuffer::get_pixel(x, y) != grayscale)
        {
            return false;
        }
    }

    return true;
}


bool column_equals(
    std::size_t x,
    std::size_t y_begin,
    std::size_t y_end,
    uint8_t grayscale)
{
    for (std::size_t y = y_begin; y < y_end; ++y)
    {
        if (framebuffer::get_pixel(x, y) != grayscale)
        {
            return false;
        }
    }

    return true;
}


bool area_equals(
    std::size_t x_begin,
    std::size_t x_end,
    std::size_t y_begin,
    std::size_t y_end,
    uint8_t grayscale)
{
    for (std::size_t y = y_begin; y < y_end; ++y)
    {
        for (std::size_t x = x_begin; x < x_end; ++x)
        {
            if (framebuffer::get_pixel(x, y) != grayscale)
            {
                return false;
            }
        }
    }

    return true;
}


std::size_t count_nonzero_pixels()
{
    std::size_t count = 0;

    for (std::size_t y = 0; y < framebuffer::height; ++y)
    {
        for (std::size_t x = 0; x < framebuffer::width; ++x)
        {
            if (framebuffer::get_pixel(x, y) != 0)
            {
                ++count;
            }
        }
    }

    return count;
}
}


int main()
{
    stdio_init_all();

    while (!stdio_usb_connected())
    {
        sleep_ms(100);
    }

    sleep_ms(200);

    std::printf("=== MEDUSA GRAPHICS TEST START ===\n");

    framebuffer::clear(0x00);
    const bool horizontal_line_drawn =
        graphics::draw_horizontal_line(
            10, 5, 6, 0x0F);
    check(
        horizontal_line_drawn &&
        row_equals(10, 16, 5, 0x0F) &&
        framebuffer::get_pixel(9, 5) == 0 &&
        framebuffer::get_pixel(16, 5) == 0 &&
        framebuffer::get_pixel(10, 4) == 0 &&
        framebuffer::get_pixel(10, 6) == 0 &&
        count_nonzero_pixels() == 6,
        "Test 1: normal horizontal line");

    framebuffer::clear(0x00);
    const bool left_clipped_line_drawn =
        graphics::draw_horizontal_line(
            -3, 10, 8, 0x07);
    check(
        left_clipped_line_drawn &&
        row_equals(0, 5, 10, 0x07) &&
        framebuffer::get_pixel(5, 10) == 0 &&
        count_nonzero_pixels() == 5,
        "Test 2: left-clipped horizontal line");

    framebuffer::clear(0x00);
    const bool right_clipped_line_drawn =
        graphics::draw_horizontal_line(
            253, 12, 10, 0x09);
    check(
        right_clipped_line_drawn &&
        row_equals(253, 256, 12, 0x09) &&
        framebuffer::get_pixel(252, 12) == 0 &&
        framebuffer::get_pixel(253, 11) == 0 &&
        framebuffer::get_pixel(253, 13) == 0 &&
        count_nonzero_pixels() == 3,
        "Test 3: right-clipped horizontal line");

    framebuffer::clear(0x00);
    const bool vertical_line_drawn =
        graphics::draw_vertical_line(
            20, 10, 5, 0x0C);
    check(
        vertical_line_drawn &&
        column_equals(20, 10, 15, 0x0C) &&
        framebuffer::get_pixel(20, 9) == 0 &&
        framebuffer::get_pixel(20, 15) == 0 &&
        count_nonzero_pixels() == 5,
        "Test 4: normal vertical line");

    framebuffer::clear(0x00);
    const bool top_clipped_line_drawn =
        graphics::draw_vertical_line(
            30, -2, 6, 0x05);
    check(
        top_clipped_line_drawn &&
        column_equals(30, 0, 4, 0x05) &&
        framebuffer::get_pixel(30, 4) == 0 &&
        count_nonzero_pixels() == 4,
        "Test 5: top-clipped vertical line");

    framebuffer::clear(0x00);
    const bool bottom_clipped_line_drawn =
        graphics::draw_vertical_line(
            40, 62, 10, 0x06);
    check(
        bottom_clipped_line_drawn &&
        column_equals(40, 62, 64, 0x06) &&
        framebuffer::get_pixel(40, 61) == 0 &&
        count_nonzero_pixels() == 2,
        "Test 6: bottom-clipped vertical line");

    framebuffer::clear(0x00);
    const bool filled_rectangle_drawn =
        graphics::fill_rectangle(
            50, 20, 4, 3, 0x0A);
    check(
        filled_rectangle_drawn &&
        area_equals(50, 54, 20, 23, 0x0A) &&
        framebuffer::get_pixel(49, 20) == 0 &&
        framebuffer::get_pixel(54, 20) == 0 &&
        framebuffer::get_pixel(50, 19) == 0 &&
        framebuffer::get_pixel(50, 23) == 0 &&
        count_nonzero_pixels() == 12,
        "Test 7: filled rectangle");

    framebuffer::clear(0x00);
    const bool clipped_fill_drawn =
        graphics::fill_rectangle(
            254, 62, 10, 10, 0x0D);
    check(
        clipped_fill_drawn &&
        area_equals(254, 256, 62, 64, 0x0D) &&
        count_nonzero_pixels() == 4,
        "Test 8: bottom-right clipped fill");

    framebuffer::clear(0x00);
    const bool rectangle_drawn =
        graphics::draw_rectangle(
            70, 15, 6, 4, 0x0F);
    check(
        rectangle_drawn &&
        row_equals(70, 76, 15, 0x0F) &&
        row_equals(70, 76, 18, 0x0F) &&
        column_equals(70, 15, 19, 0x0F) &&
        column_equals(75, 15, 19, 0x0F) &&
        framebuffer::get_pixel(71, 16) == 0 &&
        framebuffer::get_pixel(74, 17) == 0 &&
        count_nonzero_pixels() == 16,
        "Test 9: rectangle outline");

    framebuffer::clear(0x00);
    const bool single_pixel_rectangle_drawn =
        graphics::draw_rectangle(
            100, 30, 1, 1, 0x08);
    check(
        single_pixel_rectangle_drawn &&
        framebuffer::get_pixel(100, 30) == 0x08 &&
        framebuffer::get_pixel(99, 30) == 0 &&
        framebuffer::get_pixel(101, 30) == 0 &&
        framebuffer::get_pixel(100, 29) == 0 &&
        framebuffer::get_pixel(100, 31) == 0 &&
        count_nonzero_pixels() == 1,
        "Test 10: single-pixel rectangle");

    framebuffer::clear(0x00);
    const bool width_one_rectangle_drawn =
        graphics::draw_rectangle(
            110, 20, 1, 5, 0x04);
    check(
        width_one_rectangle_drawn &&
        column_equals(110, 20, 25, 0x04) &&
        framebuffer::get_pixel(110, 19) == 0 &&
        framebuffer::get_pixel(110, 25) == 0 &&
        count_nonzero_pixels() == 5,
        "Test 11: width-one rectangle");

    framebuffer::clear(0x00);
    const bool height_one_rectangle_drawn =
        graphics::draw_rectangle(
            120, 25, 7, 1, 0x03);
    check(
        height_one_rectangle_drawn &&
        row_equals(120, 127, 25, 0x03) &&
        framebuffer::get_pixel(119, 25) == 0 &&
        framebuffer::get_pixel(127, 25) == 0 &&
        count_nonzero_pixels() == 7,
        "Test 12: height-one rectangle");

    framebuffer::clear(0x00);
    const bool zero_horizontal_length =
        graphics::draw_horizontal_line(
            0, 0, 0, 15);
    const bool negative_vertical_length =
        graphics::draw_vertical_line(
            0, 0, -1, 15);
    const bool zero_fill_width =
        graphics::fill_rectangle(
            0, 0, 0, 10, 15);
    const bool zero_fill_height =
        graphics::fill_rectangle(
            0, 0, 10, 0, 15);
    const bool negative_rectangle_width =
        graphics::draw_rectangle(
            0, 0, -5, 10, 15);
    check(
        !zero_horizontal_length &&
        !negative_vertical_length &&
        !zero_fill_width &&
        !zero_fill_height &&
        !negative_rectangle_width &&
        count_nonzero_pixels() == 0,
        "Test 13: invalid dimensions");

    framebuffer::clear(0x00);
    const bool horizontal_left_outside =
        graphics::draw_horizontal_line(
            -20, 5, 10, 15);
    const bool horizontal_right_outside =
        graphics::draw_horizontal_line(
            300, 5, 10, 15);
    const bool vertical_top_outside =
        graphics::draw_vertical_line(
            5, -20, 10, 15);
    const bool vertical_bottom_outside =
        graphics::draw_vertical_line(
            5, 100, 10, 15);
    const bool fill_top_left_outside =
        graphics::fill_rectangle(
            -20, -20, 5, 5, 15);
    const bool fill_bottom_right_outside =
        graphics::fill_rectangle(
            300, 100, 5, 5, 15);
    const bool surrounding_border_outside =
        graphics::draw_rectangle(
            -10, -10, 300, 100, 15);
    check(
        !horizontal_left_outside &&
        !horizontal_right_outside &&
        !vertical_top_outside &&
        !vertical_bottom_outside &&
        !fill_top_left_outside &&
        !fill_bottom_right_outside &&
        !surrounding_border_outside &&
        count_nonzero_pixels() == 0,
        "Test 14: completely outside graphics");

    framebuffer::clear(0x00);
    const bool masked_fill_drawn =
        graphics::fill_rectangle(
            0, 0, 2, 1, 0xF4);
    check(
        masked_fill_drawn &&
        framebuffer::get_pixel(0, 0) == 4 &&
        framebuffer::get_pixel(1, 0) == 4 &&
        framebuffer::data()[0] == 0x44 &&
        count_nonzero_pixels() == 2,
        "Test 15: grayscale masking");

    framebuffer::clear(0x00);

    graphics::draw_rectangle(
        0,
        0,
        static_cast<std::int32_t>(framebuffer::width),
        static_cast<std::int32_t>(framebuffer::height),
        15);

    graphics::fill_rectangle(
        8, 5, 100, 10, 6);

    graphics::draw_rectangle(
        15, 22, 70, 30, 15);

    graphics::fill_rectangle(
        25, 30, 50, 14, 9);

    graphics::fill_rectangle(
        120, 20, 35, 30, 4);

    graphics::fill_rectangle(
        165, 20, 35, 30, 9);

    graphics::fill_rectangle(
        210, 20, 35, 30, 15);

    graphics::fill_rectangle(
        -5, 55, 20, 20, 10);

    graphics::draw_rectangle(
        245, -5, 20, 20, 15);

    std::printf("----- GRAPHICS ASCII PREVIEW -----\n");

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

    std::printf("----- END GRAPHICS PREVIEW -----\n");
    std::printf("Graphics tests complete\n");
    std::printf(
        "Passed: %u\n",
        static_cast<unsigned int>(passed));
    std::printf(
        "Failed: %u\n",
        static_cast<unsigned int>(failed));

    if (failed == 0)
    {
        std::printf("GRAPHICS TEST RESULT: PASS\n");
    }
    else
    {
        std::printf("GRAPHICS TEST RESULT: FAIL\n");
    }

    while (true)
    {
        sleep_ms(2000);
        std::printf(
            "Graphics test firmware running\n");
    }
}
