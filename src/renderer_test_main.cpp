#include "framebuffer.hpp"
#include "graphics.hpp"

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace
{
std::size_t passed = 0;
std::size_t failed = 0;

static constexpr uint8_t test_bitmap_4x3[] =
{
    0x12, 0x34,
    0x56, 0x78,
    0x9A, 0xBC
};

static constexpr uint8_t odd_bitmap_3x2[] =
{
    0x12, 0x30,
    0x45, 0x60
};

static constexpr uint8_t black_pixel_bitmap_2x1[] =
{
    0x05
};

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

static constexpr uint8_t expected_degrees_0[] =
{
    1, 2, 3, 4,
    5, 6, 7, 8,
    9, 10, 11, 12
};

static constexpr uint8_t expected_degrees_90[] =
{
    9, 5, 1,
    10, 6, 2,
    11, 7, 3,
    12, 8, 4
};

static constexpr uint8_t expected_degrees_180[] =
{
    12, 11, 10, 9,
    8, 7, 6, 5,
    4, 3, 2, 1
};

static constexpr uint8_t expected_degrees_270[] =
{
    4, 8, 12,
    3, 7, 11,
    2, 6, 10,
    1, 5, 9
};

static constexpr uint8_t expected_odd_bitmap[] =
{
    1, 2, 3,
    4, 5, 6
};

static constexpr uint8_t expected_top_left_clip[] =
{
    7, 8,
    11, 12
};

static constexpr uint8_t expected_bottom_right_clip[] =
{
    1, 2,
    5, 6
};

static constexpr uint8_t expected_black_overwrite[] =
{
    0, 5
};


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


bool framebuffer_equals(uint8_t grayscale)
{
    for (std::size_t y = 0; y < framebuffer::height; ++y)
    {
        for (std::size_t x = 0; x < framebuffer::width; ++x)
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


bool row_equals(
    std::size_t y,
    uint8_t grayscale)
{
    for (std::size_t x = 0; x < framebuffer::width; ++x)
    {
        if (framebuffer::get_pixel(x, y) != grayscale)
        {
            return false;
        }
    }

    return true;
}


std::uint32_t framebuffer_checksum()
{
    constexpr std::uint32_t fnv_offset_basis = 2166136261U;
    constexpr std::uint32_t fnv_prime = 16777619U;

    std::uint32_t checksum = fnv_offset_basis;
    const uint8_t* data = framebuffer::data();

    for (std::size_t index = 0;
         index < framebuffer::data_size();
         ++index)
    {
        checksum ^= data[index];
        checksum *= fnv_prime;
    }

    return checksum;
}


bool framebuffer_matches_region(
    std::size_t origin_x,
    std::size_t origin_y,
    std::size_t region_width,
    std::size_t region_height,
    const uint8_t* expected,
    uint8_t background)
{
    for (std::size_t y = 0; y < framebuffer::height; ++y)
    {
        for (std::size_t x = 0; x < framebuffer::width; ++x)
        {
            const bool inside_x =
                x >= origin_x &&
                x - origin_x < region_width;
            const bool inside_y =
                y >= origin_y &&
                y - origin_y < region_height;

            uint8_t expected_pixel = background;

            if (inside_x && inside_y)
            {
                expected_pixel = expected[
                    (y - origin_y) * region_width +
                    (x - origin_x)
                ];
            }

            if (framebuffer::get_pixel(x, y) != expected_pixel)
            {
                return false;
            }
        }
    }

    return true;
}


bool shallow_line_properties_ok()
{
    std::int32_t previous_y = -1;

    for (std::size_t x = 20; x <= 37; ++x)
    {
        std::size_t pixels_in_column = 0;
        std::int32_t current_y = -1;

        for (std::size_t y = 0; y < framebuffer::height; ++y)
        {
            const uint8_t pixel = framebuffer::get_pixel(x, y);

            if (pixel == 0)
            {
                continue;
            }

            if (pixel != 11 || y < 10 || y > 16)
            {
                return false;
            }

            ++pixels_in_column;
            current_y = static_cast<std::int32_t>(y);
        }

        if (pixels_in_column != 1)
        {
            return false;
        }

        if (x == 20 && current_y != 10)
        {
            return false;
        }

        if (x == 37 && current_y != 16)
        {
            return false;
        }

        if (previous_y >= 0)
        {
            const std::int32_t difference =
                current_y >= previous_y
                    ? current_y - previous_y
                    : previous_y - current_y;

            if (difference > 1)
            {
                return false;
            }
        }

        previous_y = current_y;
    }

    return count_nonzero_pixels() == 18;
}


char preview_character(uint8_t grayscale)
{
    if (grayscale == 0)
    {
        return ' ';
    }

    if (grayscale <= 5)
    {
        return '.';
    }

    if (grayscale <= 10)
    {
        return '*';
    }

    return '#';
}


void print_ascii_preview()
{
    constexpr std::size_t preview_scale = 2;
    constexpr std::size_t preview_width =
        framebuffer::width / preview_scale;

    std::printf("----- RENDERER ASCII PREVIEW -----\n");

    for (std::size_t block_y = 0;
         block_y < framebuffer::height / preview_scale;
         ++block_y)
    {
        char line[preview_width + 1] = {};

        for (std::size_t block_x = 0;
             block_x < preview_width;
             ++block_x)
        {
            uint8_t maximum_grayscale = 0;

            for (std::size_t offset_y = 0;
                 offset_y < preview_scale;
                 ++offset_y)
            {
                for (std::size_t offset_x = 0;
                     offset_x < preview_scale;
                     ++offset_x)
                {
                    const uint8_t grayscale =
                        framebuffer::get_pixel(
                            block_x * preview_scale + offset_x,
                            block_y * preview_scale + offset_y);

                    if (grayscale > maximum_grayscale)
                    {
                        maximum_grayscale = grayscale;
                    }
                }
            }

            line[block_x] =
                preview_character(maximum_grayscale);
        }

        line[preview_width] = '\0';
        std::printf("%s\n", line);
    }

    std::printf("----- END RENDERER PREVIEW -----\n");
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

    std::printf("=== MEDUSA RENDERER TEST START ===\n");

    framebuffer::clear(0);
    const bool point_line_drawn =
        graphics::draw_line(10, 10, 10, 10, 15);
    check(
        point_line_drawn &&
        framebuffer::get_pixel(10, 10) == 15 &&
        count_nonzero_pixels() == 1,
        "Test 1: single-point line");

    framebuffer::clear(0);
    const bool descending_diagonal_drawn =
        graphics::draw_line(2, 2, 7, 7, 12);
    bool descending_diagonal_correct = true;

    for (std::size_t coordinate = 2;
         coordinate <= 7;
         ++coordinate)
    {
        if (framebuffer::get_pixel(
                coordinate,
                coordinate) != 12)
        {
            descending_diagonal_correct = false;
        }
    }

    check(
        descending_diagonal_drawn &&
        descending_diagonal_correct &&
        count_nonzero_pixels() == 6,
        "Test 2: descending 45-degree line");

    framebuffer::clear(0);
    const bool ascending_diagonal_drawn =
        graphics::draw_line(2, 7, 7, 2, 9);
    bool ascending_diagonal_correct = true;

    for (std::size_t x = 2; x <= 7; ++x)
    {
        const std::size_t y = 9 - x;

        if (framebuffer::get_pixel(x, y) != 9)
        {
            ascending_diagonal_correct = false;
        }
    }

    check(
        ascending_diagonal_drawn &&
        ascending_diagonal_correct &&
        count_nonzero_pixels() == 6,
        "Test 3: ascending 45-degree line");

    framebuffer::clear(0);
    const bool forward_line_drawn =
        graphics::draw_line(2, 2, 9, 6, 15);
    const std::uint32_t forward_checksum =
        framebuffer_checksum();
    const bool forward_line_correct =
        forward_line_drawn &&
        framebuffer::get_pixel(2, 2) == 15 &&
        framebuffer::get_pixel(9, 6) == 15 &&
        count_nonzero_pixels() == 8;

    framebuffer::clear(0);
    const bool reverse_line_drawn =
        graphics::draw_line(9, 6, 2, 2, 15);
    check(
        forward_line_correct &&
        reverse_line_drawn &&
        framebuffer::get_pixel(2, 2) == 15 &&
        framebuffer::get_pixel(9, 6) == 15 &&
        count_nonzero_pixels() == 8 &&
        framebuffer_checksum() == forward_checksum,
        "Test 4: reversed endpoints match");

    framebuffer::clear(0);
    const bool shallow_line_drawn =
        graphics::draw_line(20, 10, 37, 16, 11);
    check(
        shallow_line_drawn &&
        shallow_line_properties_ok(),
        "Test 5: non-45-degree line properties");

    framebuffer::clear(0);
    const bool huge_horizontal_line_drawn =
        graphics::draw_line(
            INT32_MIN,
            20,
            INT32_MAX,
            20,
            7);
    check(
        huge_horizontal_line_drawn &&
        row_equals(20, 7) &&
        count_nonzero_pixels() == framebuffer::width,
        "Test 6: huge horizontal line clipping");

    framebuffer::clear(0);
    const bool clipped_diagonal_drawn =
        graphics::draw_line(-10, -10, 70, 70, 15);
    bool clipped_diagonal_correct = true;

    for (std::size_t coordinate = 0;
         coordinate < framebuffer::height;
         ++coordinate)
    {
        if (framebuffer::get_pixel(
                coordinate,
                coordinate) != 15)
        {
            clipped_diagonal_correct = false;
        }
    }

    check(
        clipped_diagonal_drawn &&
        clipped_diagonal_correct &&
        count_nonzero_pixels() == framebuffer::height,
        "Test 7: diagonal clipping through screen");

    framebuffer::clear(6);
    const bool offscreen_line_1 =
        graphics::draw_line(-100, -100, -20, -20, 15);
    const bool offscreen_line_2 =
        graphics::draw_line(300, 100, 400, 200, 15);
    const bool offscreen_line_3 =
        graphics::draw_line(-100, 70, 300, 70, 15);
    check(
        !offscreen_line_1 &&
        !offscreen_line_2 &&
        !offscreen_line_3 &&
        framebuffer_equals(6),
        "Test 8: fully offscreen lines rejected");

    framebuffer::clear(0);
    const bool bitmap_degrees_0_drawn =
        graphics::draw_bitmap_4bpp(
            10,
            10,
            test_bitmap_4x3,
            sizeof(test_bitmap_4x3),
            4,
            3,
            graphics::Rotation::Degrees0);
    check(
        bitmap_degrees_0_drawn &&
        framebuffer_matches_region(
            10, 10, 4, 3, expected_degrees_0, 0),
        "Test 9: 4bpp bitmap at 0 degrees");

    framebuffer::clear(0);
    const bool bitmap_degrees_90_drawn =
        graphics::draw_bitmap_4bpp(
            10,
            10,
            test_bitmap_4x3,
            sizeof(test_bitmap_4x3),
            4,
            3,
            graphics::Rotation::Degrees90);
    check(
        bitmap_degrees_90_drawn &&
        framebuffer_matches_region(
            10, 10, 3, 4, expected_degrees_90, 0),
        "Test 10: 4bpp bitmap at 90 degrees");

    framebuffer::clear(0);
    const bool bitmap_degrees_180_drawn =
        graphics::draw_bitmap_4bpp(
            10,
            10,
            test_bitmap_4x3,
            sizeof(test_bitmap_4x3),
            4,
            3,
            graphics::Rotation::Degrees180);
    check(
        bitmap_degrees_180_drawn &&
        framebuffer_matches_region(
            10, 10, 4, 3, expected_degrees_180, 0),
        "Test 11: 4bpp bitmap at 180 degrees");

    framebuffer::clear(0);
    const bool bitmap_degrees_270_drawn =
        graphics::draw_bitmap_4bpp(
            10,
            10,
            test_bitmap_4x3,
            sizeof(test_bitmap_4x3),
            4,
            3,
            graphics::Rotation::Degrees270);
    check(
        bitmap_degrees_270_drawn &&
        framebuffer_matches_region(
            10, 10, 3, 4, expected_degrees_270, 0),
        "Test 12: 4bpp bitmap at 270 degrees");

    framebuffer::clear(14);
    const bool odd_bitmap_drawn =
        graphics::draw_bitmap_4bpp(
            20,
            20,
            odd_bitmap_3x2,
            sizeof(odd_bitmap_3x2),
            3,
            2,
            graphics::Rotation::Degrees0);
    check(
        odd_bitmap_drawn &&
        framebuffer_matches_region(
            20, 20, 3, 2, expected_odd_bitmap, 14),
        "Test 13: odd-width 4bpp bitmap");

    framebuffer::clear(0);
    const bool top_left_clipped_bitmap_drawn =
        graphics::draw_bitmap_4bpp(
            -2,
            -1,
            test_bitmap_4x3,
            sizeof(test_bitmap_4x3),
            4,
            3,
            graphics::Rotation::Degrees0);
    check(
        top_left_clipped_bitmap_drawn &&
        framebuffer_matches_region(
            0, 0, 2, 2, expected_top_left_clip, 0),
        "Test 14: top-left bitmap clipping");

    framebuffer::clear(0);
    const bool bottom_right_clipped_bitmap_drawn =
        graphics::draw_bitmap_4bpp(
            254,
            62,
            test_bitmap_4x3,
            sizeof(test_bitmap_4x3),
            4,
            3,
            graphics::Rotation::Degrees0);
    check(
        bottom_right_clipped_bitmap_drawn &&
        framebuffer_matches_region(
            254,
            62,
            2,
            2,
            expected_bottom_right_clip,
            0),
        "Test 15: bottom-right bitmap clipping");

    framebuffer::clear(15);
    const bool black_pixel_bitmap_drawn =
        graphics::draw_bitmap_4bpp(
            30,
            30,
            black_pixel_bitmap_2x1,
            sizeof(black_pixel_bitmap_2x1),
            2,
            1,
            graphics::Rotation::Degrees0);
    check(
        black_pixel_bitmap_drawn &&
        framebuffer_matches_region(
            30,
            30,
            2,
            1,
            expected_black_overwrite,
            15),
        "Test 16: black bitmap pixel overwrites background");

    framebuffer::clear(6);
    const std::size_t maximum_size =
        static_cast<std::size_t>(-1);
    const bool null_bitmap_rejected =
        !graphics::draw_bitmap_4bpp(
            0, 0, nullptr, 6, 4, 3,
            graphics::Rotation::Degrees0);
    const bool zero_width_rejected =
        !graphics::draw_bitmap_4bpp(
            0, 0, test_bitmap_4x3, 6, 0, 3,
            graphics::Rotation::Degrees0);
    const bool zero_height_rejected =
        !graphics::draw_bitmap_4bpp(
            0, 0, test_bitmap_4x3, 6, 4, 0,
            graphics::Rotation::Degrees0);
    const bool short_buffer_rejected =
        !graphics::draw_bitmap_4bpp(
            0, 0, test_bitmap_4x3, 5, 4, 3,
            graphics::Rotation::Degrees0);
    const bool invalid_rotation_rejected =
        !graphics::draw_bitmap_4bpp(
            0, 0, test_bitmap_4x3, 6, 4, 3,
            static_cast<graphics::Rotation>(255));
    const bool row_size_overflow_rejected =
        !graphics::draw_bitmap_4bpp(
            0,
            0,
            test_bitmap_4x3,
            sizeof(test_bitmap_4x3),
            maximum_size,
            1,
            graphics::Rotation::Degrees0);
    const bool total_size_overflow_rejected =
        !graphics::draw_bitmap_4bpp(
            0,
            0,
            test_bitmap_4x3,
            sizeof(test_bitmap_4x3),
            3,
            maximum_size,
            graphics::Rotation::Degrees0);
    check(
        null_bitmap_rejected &&
        zero_width_rejected &&
        zero_height_rejected &&
        short_buffer_rejected &&
        invalid_rotation_rejected &&
        row_size_overflow_rejected &&
        total_size_overflow_rejected &&
        framebuffer_equals(6),
        "Test 17: invalid bitmap parameters are atomic");

    framebuffer::clear(14);
    const bool offscreen_bitmap_1 =
        graphics::draw_bitmap_4bpp(
            300, 20, test_bitmap_4x3, 6, 4, 3,
            graphics::Rotation::Degrees0);
    const bool offscreen_bitmap_2 =
        graphics::draw_bitmap_4bpp(
            -100, 20, test_bitmap_4x3, 6, 4, 3,
            graphics::Rotation::Degrees0);
    const bool offscreen_bitmap_3 =
        graphics::draw_bitmap_4bpp(
            20, 100, test_bitmap_4x3, 6, 4, 3,
            graphics::Rotation::Degrees0);
    const bool offscreen_bitmap_4 =
        graphics::draw_bitmap_4bpp(
            20, -100, test_bitmap_4x3, 6, 4, 3,
            graphics::Rotation::Degrees0);
    check(
        !offscreen_bitmap_1 &&
        !offscreen_bitmap_2 &&
        !offscreen_bitmap_3 &&
        !offscreen_bitmap_4 &&
        framebuffer_equals(14),
        "Test 18: fully offscreen bitmaps rejected");

    framebuffer::clear(0);
    bool preview_ready =
        graphics::draw_rectangle(0, 0, 256, 64, 15);
    preview_ready =
        graphics::draw_line(0, 0, 255, 63, 6) &&
        preview_ready;
    preview_ready =
        graphics::draw_line(0, 63, 255, 0, 9) &&
        preview_ready;
    preview_ready =
        graphics::draw_text(88, 28, "RENDERER", 15, 0) &&
        preview_ready;
    preview_ready =
        graphics::draw_bitmap_4bpp(
            16, 8, arrow_bitmap_8x8,
            sizeof(arrow_bitmap_8x8), 8, 8,
            graphics::Rotation::Degrees0) &&
        preview_ready;
    preview_ready =
        graphics::draw_bitmap_4bpp(
            232, 8, arrow_bitmap_8x8,
            sizeof(arrow_bitmap_8x8), 8, 8,
            graphics::Rotation::Degrees90) &&
        preview_ready;
    preview_ready =
        graphics::draw_bitmap_4bpp(
            232, 48, arrow_bitmap_8x8,
            sizeof(arrow_bitmap_8x8), 8, 8,
            graphics::Rotation::Degrees180) &&
        preview_ready;
    preview_ready =
        graphics::draw_bitmap_4bpp(
            16, 48, arrow_bitmap_8x8,
            sizeof(arrow_bitmap_8x8), 8, 8,
            graphics::Rotation::Degrees270) &&
        preview_ready;

    if (!preview_ready)
    {
        std::printf("Renderer preview drawing failed\n");
    }

    print_ascii_preview();

    std::printf("Renderer tests complete\n");
    std::printf(
        "Passed: %u\n",
        static_cast<unsigned int>(passed));
    std::printf(
        "Failed: %u\n",
        static_cast<unsigned int>(failed));

    if (failed == 0)
    {
        std::printf("RENDERER TEST RESULT: PASS\n");
    }
    else
    {
        std::printf("RENDERER TEST RESULT: FAIL\n");
    }

    while (true)
    {
        sleep_ms(2000);
        std::printf(
            "Renderer test firmware running\n");
    }
}
