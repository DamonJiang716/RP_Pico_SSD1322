#include "framebuffer.hpp"
#include "graphics.hpp"
#include "sprite.hpp"

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace
{
std::size_t passed = 0;
std::size_t failed = 0;

static constexpr std::uint8_t test_sprite_data[] =
{
    0x1B,
    0xE4
};

static constexpr sprite::Sprite2bpp test_sprite =
{
    test_sprite_data,
    sizeof(test_sprite_data),
    4,
    2
};

static constexpr std::uint8_t asymmetric_sprite_data[] =
{
    0x18,
    0xC4
};

static constexpr sprite::Sprite2bpp asymmetric_sprite =
{
    asymmetric_sprite_data,
    sizeof(asymmetric_sprite_data),
    3,
    2
};

static constexpr std::uint8_t odd_width_sprite_data[] =
{
    0x1B, 0x40,
    0xE4, 0x80
};

static constexpr sprite::Sprite2bpp odd_width_sprite =
{
    odd_width_sprite_data,
    sizeof(odd_width_sprite_data),
    5,
    2
};

static constexpr std::uint8_t mapping_sprite_data[] =
{
    0x1B
};

static constexpr sprite::Sprite2bpp mapping_sprite =
{
    mapping_sprite_data,
    sizeof(mapping_sprite_data),
    4,
    1
};

static constexpr std::uint8_t extended_sprite_data[] =
{
    0x1B,
    0xE4,
    0xFF
};

static constexpr sprite::Sprite2bpp extended_sprite =
{
    extended_sprite_data,
    sizeof(extended_sprite_data),
    4,
    2
};

static constexpr std::uint8_t preview_sprite_data[] =
{
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF,
    0x00, 0x55, 0xAA, 0xFF
};

static constexpr sprite::Sprite2bpp preview_sprite =
{
    preview_sprite_data,
    sizeof(preview_sprite_data),
    16,
    16
};

static constexpr std::uint8_t expected_degrees_0[] =
{
    0, 5, 10, 15,
    15, 10, 5, 0
};

static constexpr std::uint8_t expected_degrees_90[] =
{
    15, 0,
    10, 5,
    5, 10,
    0, 15
};

static constexpr std::uint8_t expected_degrees_180[] =
{
    0, 5, 10, 15,
    15, 10, 5, 0
};

static constexpr std::uint8_t expected_degrees_270[] =
{
    15, 0,
    10, 5,
    5, 10,
    0, 15
};

static constexpr std::uint8_t expected_asymmetric_90[] =
{
    15, 0,
    0, 5,
    5, 10
};

static constexpr std::uint8_t expected_asymmetric_180[] =
{
    5, 0, 15,
    10, 5, 0
};

static constexpr std::uint8_t expected_asymmetric_270[] =
{
    10, 5,
    5, 0,
    0, 15
};

static constexpr std::uint8_t expected_odd_width[] =
{
    0, 5, 10, 15, 5,
    15, 10, 5, 0, 10
};

static constexpr std::uint8_t expected_top_left_clip[] =
{
    5, 0
};

static constexpr std::uint8_t expected_bottom_right_clip[] =
{
    0, 5
};

static constexpr std::uint8_t expected_mapping[] =
{
    0, 5, 10, 15
};

static_assert(sprite::bits_per_pixel == 2);
static_assert(sprite::pixels_per_byte == 4);
static_assert(sizeof(test_sprite_data) == 2);
static_assert(sizeof(preview_sprite_data) == 64);


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


bool framebuffer_equals(std::uint8_t grayscale)
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


std::uint32_t framebuffer_checksum()
{
    constexpr std::uint32_t fnv_offset_basis = 2166136261U;
    constexpr std::uint32_t fnv_prime = 16777619U;

    std::uint32_t checksum = fnv_offset_basis;
    const std::uint8_t* data = framebuffer::data();

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
    const std::uint8_t* expected,
    std::uint8_t background)
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

            std::uint8_t expected_pixel = background;

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


bool draw_and_match(
    const sprite::Sprite2bpp& image,
    graphics::Rotation rotation,
    std::size_t output_width,
    std::size_t output_height,
    const std::uint8_t* expected,
    std::uint8_t background)
{
    constexpr std::int32_t origin_x = 10;
    constexpr std::int32_t origin_y = 10;

    framebuffer::clear(background);

    return
        graphics::draw_sprite_2bpp(
            origin_x,
            origin_y,
            image,
            rotation) &&
        framebuffer_matches_region(
            static_cast<std::size_t>(origin_x),
            static_cast<std::size_t>(origin_y),
            output_width,
            output_height,
            expected,
            background);
}


char preview_character(std::uint8_t grayscale)
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

    std::printf("----- SPRITE ASCII PREVIEW -----\n");

    for (std::size_t block_y = 0;
         block_y < framebuffer::height / preview_scale;
         ++block_y)
    {
        char line[preview_width + 1] = {};

        for (std::size_t block_x = 0;
             block_x < preview_width;
             ++block_x)
        {
            std::uint8_t maximum_grayscale = 0;

            for (std::size_t offset_y = 0;
                 offset_y < preview_scale;
                 ++offset_y)
            {
                for (std::size_t offset_x = 0;
                     offset_x < preview_scale;
                     ++offset_x)
                {
                    const std::uint8_t grayscale =
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

    std::printf("----- END SPRITE PREVIEW -----\n");
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

    std::printf("=== MEDUSA SPRITE TEST START ===\n");

    check(
        sprite::bits_per_pixel == 2 &&
        sprite::pixels_per_byte == 4,
        "Test 1: Sprite2bpp metadata");

    check(
        draw_and_match(
            test_sprite,
            graphics::Rotation::Degrees0,
            4,
            2,
            expected_degrees_0,
            7),
        "Test 2: 2bpp sprite at 0 degrees");

    const bool degrees_90_correct =
        draw_and_match(
            test_sprite,
            graphics::Rotation::Degrees90,
            2,
            4,
            expected_degrees_90,
            7) &&
        draw_and_match(
            asymmetric_sprite,
            graphics::Rotation::Degrees90,
            2,
            3,
            expected_asymmetric_90,
            7);
    check(
        degrees_90_correct,
        "Test 3: 2bpp sprite at 90 degrees");

    const bool degrees_180_correct =
        draw_and_match(
            test_sprite,
            graphics::Rotation::Degrees180,
            4,
            2,
            expected_degrees_180,
            7) &&
        draw_and_match(
            asymmetric_sprite,
            graphics::Rotation::Degrees180,
            3,
            2,
            expected_asymmetric_180,
            7);
    check(
        degrees_180_correct,
        "Test 4: 2bpp sprite at 180 degrees");

    const bool degrees_270_correct =
        draw_and_match(
            test_sprite,
            graphics::Rotation::Degrees270,
            2,
            4,
            expected_degrees_270,
            7) &&
        draw_and_match(
            asymmetric_sprite,
            graphics::Rotation::Degrees270,
            2,
            3,
            expected_asymmetric_270,
            7);
    check(
        degrees_270_correct,
        "Test 5: 2bpp sprite at 270 degrees");

    check(
        draw_and_match(
            test_sprite,
            graphics::Rotation::Degrees0,
            4,
            2,
            expected_degrees_0,
            15),
        "Test 6: black sprite pixels overwrite background");

    check(
        draw_and_match(
            odd_width_sprite,
            graphics::Rotation::Degrees0,
            5,
            2,
            expected_odd_width,
            13),
        "Test 7: odd-width rows and padding bits");

    framebuffer::clear(9);
    const bool top_left_clip_correct =
        graphics::draw_sprite_2bpp(
            -2,
            -1,
            test_sprite,
            graphics::Rotation::Degrees0) &&
        framebuffer_matches_region(
            0,
            0,
            2,
            1,
            expected_top_left_clip,
            9);
    check(
        top_left_clip_correct,
        "Test 8: top-left clipping");

    framebuffer::clear(8);
    const bool bottom_right_clip_correct =
        graphics::draw_sprite_2bpp(
            254,
            63,
            test_sprite,
            graphics::Rotation::Degrees0) &&
        framebuffer_matches_region(
            254,
            63,
            2,
            1,
            expected_bottom_right_clip,
            8);
    check(
        bottom_right_clip_correct,
        "Test 9: bottom-right clipping");

    framebuffer::clear(6);
    const std::uint32_t offscreen_checksum =
        framebuffer_checksum();
    const bool offscreen_rejected =
        !graphics::draw_sprite_2bpp(
            -100,
            20,
            test_sprite,
            graphics::Rotation::Degrees0) &&
        !graphics::draw_sprite_2bpp(
            300,
            20,
            test_sprite,
            graphics::Rotation::Degrees0) &&
        !graphics::draw_sprite_2bpp(
            20,
            -100,
            test_sprite,
            graphics::Rotation::Degrees0) &&
        !graphics::draw_sprite_2bpp(
            20,
            100,
            test_sprite,
            graphics::Rotation::Degrees0);
    check(
        offscreen_rejected &&
        framebuffer_checksum() == offscreen_checksum,
        "Test 10: fully offscreen sprites are atomic");

    framebuffer::clear(12);
    const std::size_t maximum_size =
        std::numeric_limits<std::size_t>::max();
    const sprite::Sprite2bpp null_sprite =
    {
        nullptr,
        sizeof(test_sprite_data),
        4,
        2
    };
    const sprite::Sprite2bpp zero_width_sprite =
    {
        test_sprite_data,
        sizeof(test_sprite_data),
        0,
        2
    };
    const sprite::Sprite2bpp zero_height_sprite =
    {
        test_sprite_data,
        sizeof(test_sprite_data),
        4,
        0
    };
    const sprite::Sprite2bpp short_sprite =
    {
        test_sprite_data,
        sizeof(test_sprite_data) - 1,
        4,
        2
    };
    const sprite::Sprite2bpp row_size_overflow_sprite =
    {
        test_sprite_data,
        maximum_size,
        maximum_size,
        1
    };
    const sprite::Sprite2bpp total_size_overflow_sprite =
    {
        test_sprite_data,
        maximum_size,
        5,
        maximum_size
    };

    const bool invalid_parameters_rejected =
        !graphics::draw_sprite_2bpp(
            0, 0, null_sprite,
            graphics::Rotation::Degrees0) &&
        !graphics::draw_sprite_2bpp(
            0, 0, zero_width_sprite,
            graphics::Rotation::Degrees0) &&
        !graphics::draw_sprite_2bpp(
            0, 0, zero_height_sprite,
            graphics::Rotation::Degrees0) &&
        !graphics::draw_sprite_2bpp(
            0, 0, short_sprite,
            graphics::Rotation::Degrees0) &&
        !graphics::draw_sprite_2bpp(
            0, 0, test_sprite,
            static_cast<graphics::Rotation>(255)) &&
        !graphics::draw_sprite_2bpp(
            0, 0, row_size_overflow_sprite,
            graphics::Rotation::Degrees0) &&
        !graphics::draw_sprite_2bpp(
            0, 0, total_size_overflow_sprite,
            graphics::Rotation::Degrees0);
    check(
        invalid_parameters_rejected &&
        framebuffer_equals(12),
        "Test 11: invalid parameters are atomic");

    check(
        draw_and_match(
            mapping_sprite,
            graphics::Rotation::Degrees0,
            4,
            1,
            expected_mapping,
            7),
        "Test 12: grayscale maps to 0, 5, 10, 15");

    check(
        draw_and_match(
            extended_sprite,
            graphics::Rotation::Degrees0,
            4,
            2,
            expected_degrees_0,
            7),
        "Test 13: extra source bytes are accepted");

    framebuffer::clear(0);
    bool preview_ready =
        graphics::draw_sprite_2bpp(
            16,
            16,
            preview_sprite,
            graphics::Rotation::Degrees0);
    preview_ready =
        graphics::draw_sprite_2bpp(
            72,
            16,
            preview_sprite,
            graphics::Rotation::Degrees90) &&
        preview_ready;
    preview_ready =
        graphics::draw_sprite_2bpp(
            136,
            16,
            preview_sprite,
            graphics::Rotation::Degrees180) &&
        preview_ready;
    preview_ready =
        graphics::draw_sprite_2bpp(
            200,
            16,
            preview_sprite,
            graphics::Rotation::Degrees270) &&
        preview_ready;

    if (!preview_ready)
    {
        std::printf("Sprite preview drawing failed\n");
    }

    print_ascii_preview();

    std::printf("Sprite tests complete\n");
    std::printf(
        "Passed: %u\n",
        static_cast<unsigned int>(passed));
    std::printf(
        "Failed: %u\n",
        static_cast<unsigned int>(failed));

    if (failed == 0)
    {
        std::printf("SPRITE TEST RESULT: PASS\n");
    }
    else
    {
        std::printf("SPRITE TEST RESULT: FAIL\n");
    }

    while (true)
    {
        sleep_ms(2000);
        std::printf("Sprite test firmware running\n");
    }
}
