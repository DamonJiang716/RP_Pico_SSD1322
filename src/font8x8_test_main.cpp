#include "font8x8.hpp"
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


bool glyph_has_clear_margins(
    const font8x8::Glyph& glyph)
{
    for (uint8_t row_byte : glyph)
    {
        if ((row_byte & 0x81U) != 0)
        {
            return false;
        }
    }

    return true;
}


bool glyph_is_nonblank(
    const font8x8::Glyph& glyph)
{
    for (uint8_t row_byte : glyph)
    {
        if (row_byte != 0)
        {
            return true;
        }
    }

    return false;
}


bool framebuffer_equals(uint8_t grayscale)
{
    grayscale &= 0x0F;

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


bool area_contains_value(
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
            if (framebuffer::get_pixel(x, y) == grayscale)
            {
                return true;
            }
        }
    }

    return false;
}


bool framebuffer_matches_char(
    std::int32_t character_x,
    std::int32_t character_y,
    char character,
    uint8_t foreground_grayscale,
    uint8_t background_grayscale,
    uint8_t outside_grayscale)
{
    const font8x8::Glyph* glyph =
        font8x8::find_glyph(character);

    if (glyph == nullptr)
    {
        return false;
    }

    foreground_grayscale &= 0x0F;
    background_grayscale &= 0x0F;
    outside_grayscale &= 0x0F;

    for (std::size_t y = 0; y < framebuffer::height; ++y)
    {
        for (std::size_t x = 0; x < framebuffer::width; ++x)
        {
            const std::int64_t relative_x =
                static_cast<std::int64_t>(x) - character_x;
            const std::int64_t relative_y =
                static_cast<std::int64_t>(y) - character_y;

            uint8_t expected = outside_grayscale;

            if (relative_x >= 0 &&
                relative_x <
                    static_cast<std::int64_t>(font8x8::glyph_width) &&
                relative_y >= 0 &&
                relative_y <
                    static_cast<std::int64_t>(font8x8::glyph_height))
            {
                const std::size_t row =
                    static_cast<std::size_t>(relative_y);
                const std::size_t column =
                    static_cast<std::size_t>(relative_x);
                const uint8_t mask =
                    static_cast<uint8_t>(0x80U >> column);

                expected =
                    ((*glyph)[row] & mask) != 0
                        ? foreground_grayscale
                        : background_grayscale;
            }

            if (framebuffer::get_pixel(x, y) != expected)
            {
                return false;
            }
        }
    }

    return true;
}


std::size_t text_length(const char* text)
{
    std::size_t length = 0;

    while (text[length] != '\0')
    {
        ++length;
    }

    return length;
}


bool framebuffer_matches_text(
    std::int32_t text_x,
    std::int32_t text_y,
    const char* text,
    uint8_t foreground_grayscale,
    uint8_t background_grayscale,
    uint8_t outside_grayscale)
{
    const std::size_t character_count =
        text_length(text);
    const std::int64_t text_width =
        static_cast<std::int64_t>(character_count) *
        static_cast<std::int64_t>(font8x8::character_advance);

    foreground_grayscale &= 0x0F;
    background_grayscale &= 0x0F;
    outside_grayscale &= 0x0F;

    for (std::size_t y = 0; y < framebuffer::height; ++y)
    {
        for (std::size_t x = 0; x < framebuffer::width; ++x)
        {
            const std::int64_t relative_x =
                static_cast<std::int64_t>(x) - text_x;
            const std::int64_t relative_y =
                static_cast<std::int64_t>(y) - text_y;

            uint8_t expected = outside_grayscale;

            if (relative_x >= 0 &&
                relative_x < text_width &&
                relative_y >= 0 &&
                relative_y <
                    static_cast<std::int64_t>(font8x8::glyph_height))
            {
                const std::size_t character_index =
                    static_cast<std::size_t>(
                        relative_x /
                        static_cast<std::int64_t>(
                            font8x8::character_advance));
                const std::size_t column =
                    static_cast<std::size_t>(
                        relative_x %
                        static_cast<std::int64_t>(
                            font8x8::character_advance));
                const std::size_t row =
                    static_cast<std::size_t>(relative_y);
                const font8x8::Glyph* glyph =
                    font8x8::find_glyph(text[character_index]);

                if (glyph == nullptr)
                {
                    return false;
                }

                const uint8_t mask =
                    static_cast<uint8_t>(0x80U >> column);

                expected =
                    ((*glyph)[row] & mask) != 0
                        ? foreground_grayscale
                        : background_grayscale;
            }

            if (framebuffer::get_pixel(x, y) != expected)
            {
                return false;
            }
        }
    }

    return true;
}


void print_glyph(char character)
{
    const font8x8::Glyph* glyph =
        font8x8::find_glyph(character);

    if (glyph == nullptr)
    {
        return;
    }

    std::printf("Glyph %c:\n", character);

    for (std::size_t row = 0;
         row < font8x8::glyph_height;
         ++row)
    {
        for (std::size_t column = 0;
             column < font8x8::glyph_width;
             ++column)
        {
            const uint8_t mask =
                static_cast<uint8_t>(0x80U >> column);
            const bool pixel_on =
                ((*glyph)[row] & mask) != 0;

            std::putchar(pixel_on ? '#' : '.');
        }

        std::putchar('\n');
    }
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

    std::printf("=== MEDUSA FONT8X8 TEST START ===\n");

    check(
        font8x8::glyph_width == 8 &&
        font8x8::glyph_height == 8 &&
        font8x8::glyph_byte_count == 8 &&
        font8x8::character_advance == 8,
        "Test 1: font metadata");

    bool supported_ranges = true;

    for (char character = '0'; character <= '9'; ++character)
    {
        if (!font8x8::is_supported(character))
        {
            supported_ranges = false;
        }
    }

    for (char character = 'A'; character <= 'Z'; ++character)
    {
        if (!font8x8::is_supported(character))
        {
            supported_ranges = false;
        }
    }

    for (char character = 'a'; character <= 'z'; ++character)
    {
        if (!font8x8::is_supported(character))
        {
            supported_ranges = false;
        }
    }

    check(
        supported_ranges &&
        font8x8::is_supported(' ') &&
        !font8x8::is_supported('!') &&
        !font8x8::is_supported('?') &&
        !font8x8::is_supported(':') &&
        !font8x8::is_supported('\n') &&
        !font8x8::is_supported('\t') &&
        font8x8::find_glyph('0') ==
            &font8x8::digit_glyphs[0] &&
        font8x8::find_glyph('9') ==
            &font8x8::digit_glyphs[9] &&
        font8x8::find_glyph('A') ==
            &font8x8::uppercase_glyphs[0] &&
        font8x8::find_glyph('Z') ==
            &font8x8::uppercase_glyphs[25] &&
        font8x8::find_glyph('a') ==
            &font8x8::lowercase_glyphs[0] &&
        font8x8::find_glyph('z') ==
            &font8x8::lowercase_glyphs[25],
        "Test 2: supported character ranges");

    bool clear_margins = true;
    std::size_t margin_checked_glyphs = 0;

    for (const font8x8::Glyph& glyph : font8x8::digit_glyphs)
    {
        clear_margins =
            glyph_has_clear_margins(glyph) && clear_margins;
        ++margin_checked_glyphs;
    }

    for (const font8x8::Glyph& glyph : font8x8::uppercase_glyphs)
    {
        clear_margins =
            glyph_has_clear_margins(glyph) && clear_margins;
        ++margin_checked_glyphs;
    }

    for (const font8x8::Glyph& glyph : font8x8::lowercase_glyphs)
    {
        clear_margins =
            glyph_has_clear_margins(glyph) && clear_margins;
        ++margin_checked_glyphs;
    }

    clear_margins =
        glyph_has_clear_margins(font8x8::space_glyph) &&
        clear_margins;
    ++margin_checked_glyphs;

    check(
        clear_margins && margin_checked_glyphs == 63,
        "Test 3: fixed blank margin columns");

    bool space_is_blank = true;

    for (uint8_t row_byte : font8x8::space_glyph)
    {
        if (row_byte != 0)
        {
            space_is_blank = false;
        }
    }

    check(
        space_is_blank,
        "Test 4: blank space glyph");

    bool nonspace_glyphs_are_visible = true;
    std::size_t visible_glyph_count = 0;

    for (const font8x8::Glyph& glyph : font8x8::digit_glyphs)
    {
        nonspace_glyphs_are_visible =
            glyph_is_nonblank(glyph) &&
            nonspace_glyphs_are_visible;
        ++visible_glyph_count;
    }

    for (const font8x8::Glyph& glyph : font8x8::uppercase_glyphs)
    {
        nonspace_glyphs_are_visible =
            glyph_is_nonblank(glyph) &&
            nonspace_glyphs_are_visible;
        ++visible_glyph_count;
    }

    for (const font8x8::Glyph& glyph : font8x8::lowercase_glyphs)
    {
        nonspace_glyphs_are_visible =
            glyph_is_nonblank(glyph) &&
            nonspace_glyphs_are_visible;
        ++visible_glyph_count;
    }

    check(
        nonspace_glyphs_are_visible &&
        visible_glyph_count == 62,
        "Test 5: nonspace glyphs are visible");

    framebuffer::clear(0);
    const bool character_a_drawn =
        graphics::draw_char(
            0, 0, 'A', 15, 0);
    check(
        character_a_drawn &&
        framebuffer_matches_char(
            0, 0, 'A', 15, 0, 0) &&
        column_equals(0, 0, 8, 0) &&
        column_equals(7, 0, 8, 0),
        "Test 6: draw character A");

    framebuffer::clear(0);
    const bool foreground_background_drawn =
        graphics::draw_char(
            10, 10, 'A', 12, 3);
    check(
        foreground_background_drawn &&
        framebuffer_matches_char(
            10, 10, 'A', 12, 3, 0) &&
        column_equals(10, 10, 18, 3) &&
        column_equals(17, 10, 18, 3),
        "Test 7: foreground and background grayscale");

    framebuffer::clear(0);
    const bool adjacent_text_drawn =
        graphics::draw_text(
            0, 0, "AB", 15, 0);
    check(
        adjacent_text_drawn &&
        framebuffer_matches_text(
            0, 0, "AB", 15, 0, 0) &&
        column_equals(7, 0, 8, 0) &&
        column_equals(8, 0, 8, 0) &&
        area_contains_value(0, 8, 0, 8, 15) &&
        area_contains_value(8, 16, 0, 8, 15),
        "Test 8: adjacent character spacing");

    framebuffer::clear(0);
    const bool mixed_text_drawn =
        graphics::draw_text(
            5, 20, "A1z9", 15, 0);
    check(
        mixed_text_drawn &&
        framebuffer_matches_text(
            5, 20, "A1z9", 15, 0, 0),
        "Test 9: digits and mixed-case text");

    framebuffer::clear(15);
    const bool space_drawn =
        graphics::draw_char(
            20, 10, ' ', 15, 2);
    check(
        space_drawn &&
        framebuffer_matches_char(
            20, 10, ' ', 15, 2, 15),
        "Test 10: space draws background");

    framebuffer::clear(0);
    const bool top_left_clipped =
        graphics::draw_char(
            -3, -2, 'A', 15, 0);
    check(
        top_left_clipped &&
        framebuffer_matches_char(
            -3, -2, 'A', 15, 0, 0),
        "Test 11: top-left character clipping");

    framebuffer::clear(0);
    const bool bottom_right_clipped =
        graphics::draw_char(
            252, 60, 'B', 15, 0);
    check(
        bottom_right_clipped &&
        framebuffer_matches_char(
            252, 60, 'B', 15, 0, 0),
        "Test 12: bottom-right character clipping");

    framebuffer::clear(0);
    const bool character_left_outside =
        graphics::draw_char(
            -20, 0, 'A', 15, 0);
    const bool character_right_outside =
        graphics::draw_char(
            300, 0, 'A', 15, 0);
    const bool character_top_outside =
        graphics::draw_char(
            0, -20, 'A', 15, 0);
    const bool character_bottom_outside =
        graphics::draw_char(
            0, 100, 'A', 15, 0);
    check(
        !character_left_outside &&
        !character_right_outside &&
        !character_top_outside &&
        !character_bottom_outside &&
        framebuffer_equals(0),
        "Test 13: character completely outside");

    framebuffer::clear(7);
    const bool unsupported_character_drawn =
        graphics::draw_char(
            0, 0, '!', 15, 0);
    check(
        !unsupported_character_drawn &&
        framebuffer_equals(7),
        "Test 14: unsupported character rejection");

    framebuffer::clear(5);
    const bool invalid_text_drawn =
        graphics::draw_text(
            0, 0, "ABC!DEF", 15, 0);
    check(
        !invalid_text_drawn &&
        framebuffer_equals(5),
        "Test 15: atomic invalid text rejection");

    framebuffer::clear(6);
    const bool null_text_drawn =
        graphics::draw_text(
            0, 0, nullptr, 15, 0);
    const bool empty_text_drawn =
        graphics::draw_text(
            0, 0, "", 15, 0);
    check(
        !null_text_drawn &&
        !empty_text_drawn &&
        framebuffer_equals(6),
        "Test 16: null and empty text rejection");

    framebuffer::clear(0);
    const bool masked_grayscale_drawn =
        graphics::draw_char(
            0, 0, 'A', 0xFC, 0xF3);
    check(
        masked_grayscale_drawn &&
        framebuffer_matches_char(
            0, 0, 'A', 12, 3, 0),
        "Test 17: grayscale high-bit masking");

    framebuffer::clear(0);
    const bool right_clipped_text_drawn =
        graphics::draw_text(
            248, 0, "ABC", 15, 0);
    check(
        right_clipped_text_drawn &&
        framebuffer_matches_text(
            248, 0, "ABC", 15, 0, 0),
        "Test 18: long text right clipping");

    framebuffer::clear(0);
    const bool text_outside_drawn =
        graphics::draw_text(
            300, 20, "HELLO", 15, 0);
    check(
        !text_outside_drawn &&
        framebuffer_equals(0),
        "Test 19: text completely outside");

    print_glyph('A');
    print_glyph('M');
    print_glyph('0');
    print_glyph('8');
    print_glyph('a');
    print_glyph('g');

    framebuffer::clear(0x00);

    graphics::draw_text(
        0, 0, "0123456789", 15, 0);
    graphics::draw_text(
        0, 16, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 15, 0);
    graphics::draw_text(
        0, 32, "abcdefghijklmnopqrstuvwxyz", 15, 0);
    graphics::draw_text(
        0, 48, "A1B2C3 Z9", 15, 0);

    std::printf("----- FONT8X8 ASCII PREVIEW -----\n");

    constexpr std::size_t preview_scale = 2;

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

    std::printf("----- END FONT8X8 PREVIEW -----\n");
    std::printf("Font8x8 tests complete\n");
    std::printf(
        "Passed: %u\n",
        static_cast<unsigned int>(passed));
    std::printf(
        "Failed: %u\n",
        static_cast<unsigned int>(failed));

    if (failed == 0)
    {
        std::printf("FONT8X8 TEST RESULT: PASS\n");
    }
    else
    {
        std::printf("FONT8X8 TEST RESULT: FAIL\n");
    }

    while (true)
    {
        sleep_ms(2000);
        std::printf(
            "Font8x8 test firmware running\n");
    }
}
