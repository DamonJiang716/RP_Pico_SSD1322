#include "graphics.hpp"
#include "framebuffer.hpp"
#include "font8x8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
constexpr std::int32_t screen_width =
    static_cast<std::int32_t>(framebuffer::width);

constexpr std::int32_t screen_height =
    static_cast<std::int32_t>(framebuffer::height);


bool clip_span(
    std::int32_t origin,
    std::int32_t length,
    std::int32_t limit,
    std::int32_t& visible_begin,
    std::int32_t& visible_end)
{
    if (length <= 0)
    {
        return false;
    }

    const std::int64_t origin_64 = origin;
    const std::int64_t end_64 =
        origin_64 + static_cast<std::int64_t>(length);

    if (end_64 <= 0 || origin_64 >= limit)
    {
        return false;
    }

    const std::int64_t visible_begin_64 =
        std::max<std::int64_t>(0, origin_64);
    const std::int64_t visible_end_64 =
        std::min<std::int64_t>(limit, end_64);

    if (visible_begin_64 >= visible_end_64)
    {
        return false;
    }

    visible_begin =
        static_cast<std::int32_t>(visible_begin_64);
    visible_end =
        static_cast<std::int32_t>(visible_end_64);

    return true;
}


struct ClipFraction
{
    std::uint64_t numerator;
    std::uint64_t denominator;
};


bool fraction_less(
    const ClipFraction& left,
    const ClipFraction& right)
{
    return
        left.numerator * right.denominator <
        right.numerator * left.denominator;
}


bool update_clip_interval(
    std::int64_t p,
    std::int64_t q,
    ClipFraction& entering,
    ClipFraction& leaving)
{
    if (p == 0)
    {
        return q >= 0;
    }

    if (p < 0)
    {
        if (q >= 0)
        {
            return true;
        }

        const ClipFraction candidate =
        {
            static_cast<std::uint64_t>(-q),
            static_cast<std::uint64_t>(-p)
        };

        if (fraction_less(leaving, candidate))
        {
            return false;
        }

        if (fraction_less(entering, candidate))
        {
            entering = candidate;
        }

        return true;
    }

    if (q < 0)
    {
        return false;
    }

    const ClipFraction candidate =
    {
        static_cast<std::uint64_t>(q),
        static_cast<std::uint64_t>(p)
    };

    if (fraction_less(candidate, entering))
    {
        return false;
    }

    if (fraction_less(candidate, leaving))
    {
        leaving = candidate;
    }

    return true;
}


std::int64_t interpolate_coordinate(
    std::int64_t origin,
    std::int64_t delta,
    const ClipFraction& fraction)
{
    const std::uint64_t magnitude =
        delta < 0
            ? static_cast<std::uint64_t>(-delta)
            : static_cast<std::uint64_t>(delta);

    const std::uint64_t scaled =
        magnitude * fraction.numerator /
        fraction.denominator;

    if (delta < 0)
    {
        return origin - static_cast<std::int64_t>(scaled);
    }

    return origin + static_cast<std::int64_t>(scaled);
}


bool clip_line_to_screen(
    std::int32_t x0,
    std::int32_t y0,
    std::int32_t x1,
    std::int32_t y1,
    std::int32_t& clipped_x0,
    std::int32_t& clipped_y0,
    std::int32_t& clipped_x1,
    std::int32_t& clipped_y1)
{
    const std::int64_t start_x = x0;
    const std::int64_t start_y = y0;
    const std::int64_t delta_x =
        static_cast<std::int64_t>(x1) - start_x;
    const std::int64_t delta_y =
        static_cast<std::int64_t>(y1) - start_y;

    ClipFraction entering = {0, 1};
    ClipFraction leaving = {1, 1};

    if (!update_clip_interval(
            -delta_x,
            start_x,
            entering,
            leaving) ||
        !update_clip_interval(
            delta_x,
            static_cast<std::int64_t>(screen_width - 1) -
                start_x,
            entering,
            leaving) ||
        !update_clip_interval(
            -delta_y,
            start_y,
            entering,
            leaving) ||
        !update_clip_interval(
            delta_y,
            static_cast<std::int64_t>(screen_height - 1) -
                start_y,
            entering,
            leaving))
    {
        return false;
    }

    const std::int64_t clipped_start_x =
        interpolate_coordinate(start_x, delta_x, entering);
    const std::int64_t clipped_start_y =
        interpolate_coordinate(start_y, delta_y, entering);
    const std::int64_t clipped_end_x =
        interpolate_coordinate(start_x, delta_x, leaving);
    const std::int64_t clipped_end_y =
        interpolate_coordinate(start_y, delta_y, leaving);

    if (clipped_start_x < 0 ||
        clipped_start_x >= screen_width ||
        clipped_start_y < 0 ||
        clipped_start_y >= screen_height ||
        clipped_end_x < 0 ||
        clipped_end_x >= screen_width ||
        clipped_end_y < 0 ||
        clipped_end_y >= screen_height)
    {
        return false;
    }

    clipped_x0 = static_cast<std::int32_t>(clipped_start_x);
    clipped_y0 = static_cast<std::int32_t>(clipped_start_y);
    clipped_x1 = static_cast<std::int32_t>(clipped_end_x);
    clipped_y1 = static_cast<std::int32_t>(clipped_end_y);

    return true;
}


uint8_t read_4bpp_pixel(
    const uint8_t* bitmap,
    std::size_t row_bytes,
    std::size_t source_x,
    std::size_t source_y)
{
    const std::size_t source_index =
        source_y * row_bytes + source_x / 2;
    const uint8_t packed_pixels = bitmap[source_index];

    if ((source_x & 1U) == 0)
    {
        return static_cast<uint8_t>(packed_pixels >> 4);
    }

    return static_cast<uint8_t>(packed_pixels & 0x0F);
}


uint8_t read_2bpp_pixel(
    const uint8_t* data,
    std::size_t row_bytes,
    std::size_t source_x,
    std::size_t source_y)
{
    const std::size_t byte_index =
        source_y * row_bytes + source_x / 4;
    const unsigned int shift =
        6U -
        2U * static_cast<unsigned int>(
            source_x % 4);

    return static_cast<uint8_t>(
        (data[byte_index] >> shift) &
        0x03U);
}
}

namespace graphics
{
bool draw_horizontal_line(
    std::int32_t x,
    std::int32_t y,
    std::int32_t length,
    uint8_t grayscale)
{
    if (y < 0 || y >= screen_height)
    {
        return false;
    }

    std::int32_t visible_begin = 0;
    std::int32_t visible_end = 0;

    if (!clip_span(
            x,
            length,
            screen_width,
            visible_begin,
            visible_end))
    {
        return false;
    }

    bool drew_pixel = false;

    for (std::int32_t visible_x = visible_begin;
         visible_x < visible_end;
         ++visible_x)
    {
        if (framebuffer::set_pixel(
                static_cast<std::size_t>(visible_x),
                static_cast<std::size_t>(y),
                grayscale))
        {
            drew_pixel = true;
        }
    }

    return drew_pixel;
}


bool draw_vertical_line(
    std::int32_t x,
    std::int32_t y,
    std::int32_t length,
    uint8_t grayscale)
{
    if (x < 0 || x >= screen_width)
    {
        return false;
    }

    std::int32_t visible_begin = 0;
    std::int32_t visible_end = 0;

    if (!clip_span(
            y,
            length,
            screen_height,
            visible_begin,
            visible_end))
    {
        return false;
    }

    bool drew_pixel = false;

    for (std::int32_t visible_y = visible_begin;
         visible_y < visible_end;
         ++visible_y)
    {
        if (framebuffer::set_pixel(
                static_cast<std::size_t>(x),
                static_cast<std::size_t>(visible_y),
                grayscale))
        {
            drew_pixel = true;
        }
    }

    return drew_pixel;
}


bool draw_line(
    std::int32_t x0,
    std::int32_t y0,
    std::int32_t x1,
    std::int32_t y1,
    uint8_t grayscale)
{
    if (x0 > x1 || (x0 == x1 && y0 > y1))
    {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    std::int32_t clipped_x0 = 0;
    std::int32_t clipped_y0 = 0;
    std::int32_t clipped_x1 = 0;
    std::int32_t clipped_y1 = 0;

    if (!clip_line_to_screen(
            x0,
            y0,
            x1,
            y1,
            clipped_x0,
            clipped_y0,
            clipped_x1,
            clipped_y1))
    {
        return false;
    }

    const std::int32_t delta_x =
        clipped_x1 >= clipped_x0
            ? clipped_x1 - clipped_x0
            : clipped_x0 - clipped_x1;
    const std::int32_t delta_y =
        clipped_y1 >= clipped_y0
            ? clipped_y1 - clipped_y0
            : clipped_y0 - clipped_y1;
    const std::int32_t step_x =
        clipped_x0 < clipped_x1
            ? 1
            : (clipped_x0 > clipped_x1 ? -1 : 0);
    const std::int32_t step_y =
        clipped_y0 < clipped_y1
            ? 1
            : (clipped_y0 > clipped_y1 ? -1 : 0);
    const std::int32_t maximum_steps =
        std::max(delta_x, delta_y) + 1;

    std::int32_t current_x = clipped_x0;
    std::int32_t current_y = clipped_y0;
    std::int32_t error = delta_x - delta_y;
    bool drew_pixel = false;

    for (std::int32_t step = 0;
         step < maximum_steps;
         ++step)
    {
        if (current_x < 0 ||
            current_x >= screen_width ||
            current_y < 0 ||
            current_y >= screen_height)
        {
            return drew_pixel;
        }

        if (framebuffer::set_pixel(
                static_cast<std::size_t>(current_x),
                static_cast<std::size_t>(current_y),
                grayscale))
        {
            drew_pixel = true;
        }

        if (current_x == clipped_x1 &&
            current_y == clipped_y1)
        {
            return drew_pixel;
        }

        const std::int32_t doubled_error = error * 2;

        if (doubled_error > -delta_y)
        {
            error -= delta_y;
            current_x += step_x;
        }

        if (doubled_error < delta_x)
        {
            error += delta_x;
            current_y += step_y;
        }
    }

    return drew_pixel;
}


bool draw_rectangle(
    std::int32_t x,
    std::int32_t y,
    std::int32_t width,
    std::int32_t height,
    uint8_t grayscale)
{
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    if (width == 1 && height == 1)
    {
        return draw_horizontal_line(
            x,
            y,
            1,
            grayscale);
    }

    if (height == 1)
    {
        return draw_horizontal_line(
            x,
            y,
            width,
            grayscale);
    }

    if (width == 1)
    {
        return draw_vertical_line(
            x,
            y,
            height,
            grayscale);
    }

    const std::int64_t right =
        static_cast<std::int64_t>(x) +
        static_cast<std::int64_t>(width) - 1;
    const std::int64_t bottom =
        static_cast<std::int64_t>(y) +
        static_cast<std::int64_t>(height) - 1;

    bool drew_pixel = false;

    if (draw_horizontal_line(
            x,
            y,
            width,
            grayscale))
    {
        drew_pixel = true;
    }

    if (bottom >= 0 && bottom < screen_height)
    {
        if (draw_horizontal_line(
                x,
                static_cast<std::int32_t>(bottom),
                width,
                grayscale))
        {
            drew_pixel = true;
        }
    }

    if (draw_vertical_line(
            x,
            y,
            height,
            grayscale))
    {
        drew_pixel = true;
    }

    if (right >= 0 && right < screen_width)
    {
        if (draw_vertical_line(
                static_cast<std::int32_t>(right),
                y,
                height,
                grayscale))
        {
            drew_pixel = true;
        }
    }

    return drew_pixel;
}


bool fill_rectangle(
    std::int32_t x,
    std::int32_t y,
    std::int32_t width,
    std::int32_t height,
    uint8_t grayscale)
{
    std::int32_t visible_x_begin = 0;
    std::int32_t visible_x_end = 0;
    std::int32_t visible_y_begin = 0;
    std::int32_t visible_y_end = 0;

    if (!clip_span(
            x,
            width,
            screen_width,
            visible_x_begin,
            visible_x_end) ||
        !clip_span(
            y,
            height,
            screen_height,
            visible_y_begin,
            visible_y_end))
    {
        return false;
    }

    bool drew_pixel = false;

    for (std::int32_t visible_y = visible_y_begin;
         visible_y < visible_y_end;
         ++visible_y)
    {
        for (std::int32_t visible_x = visible_x_begin;
             visible_x < visible_x_end;
             ++visible_x)
        {
            if (framebuffer::set_pixel(
                    static_cast<std::size_t>(visible_x),
                    static_cast<std::size_t>(visible_y),
                    grayscale))
            {
                drew_pixel = true;
            }
        }
    }

    return drew_pixel;
}


bool draw_char(
    std::int32_t x,
    std::int32_t y,
    char character,
    uint8_t foreground_grayscale,
    uint8_t background_grayscale)
{
    const font8x8::Glyph* glyph =
        font8x8::find_glyph(character);

    if (glyph == nullptr)
    {
        return false;
    }

    const std::int64_t character_left = x;
    const std::int64_t character_top = y;
    const std::int64_t character_right =
        character_left +
        static_cast<std::int64_t>(font8x8::glyph_width) - 1;
    const std::int64_t character_bottom =
        character_top +
        static_cast<std::int64_t>(font8x8::glyph_height) - 1;

    if (character_right < 0 ||
        character_left >= screen_width ||
        character_bottom < 0 ||
        character_top >= screen_height)
    {
        return false;
    }

    bool drew_pixel = false;

    for (std::size_t row = 0;
         row < font8x8::glyph_height;
         ++row)
    {
        const std::int64_t pixel_y =
            character_top + static_cast<std::int64_t>(row);

        if (pixel_y < 0 || pixel_y >= screen_height)
        {
            continue;
        }

        for (std::size_t column = 0;
             column < font8x8::glyph_width;
             ++column)
        {
            const std::int64_t pixel_x =
                character_left +
                static_cast<std::int64_t>(column);

            if (pixel_x < 0 || pixel_x >= screen_width)
            {
                continue;
            }

            const uint8_t mask =
                static_cast<uint8_t>(0x80U >> column);
            const bool pixel_on =
                ((*glyph)[row] & mask) != 0;
            const uint8_t grayscale =
                pixel_on
                    ? foreground_grayscale
                    : background_grayscale;

            if (framebuffer::set_pixel(
                    static_cast<std::size_t>(pixel_x),
                    static_cast<std::size_t>(pixel_y),
                    grayscale))
            {
                drew_pixel = true;
            }
        }
    }

    return drew_pixel;
}


bool draw_text(
    std::int32_t x,
    std::int32_t y,
    const char* text,
    uint8_t foreground_grayscale,
    uint8_t background_grayscale)
{
    if (text == nullptr || text[0] == '\0')
    {
        return false;
    }

    constexpr std::int64_t advance =
        static_cast<std::int64_t>(
            font8x8::character_advance);
    constexpr std::int64_t maximum_character_count =
        std::numeric_limits<std::int64_t>::max() /
        advance;

    std::int64_t character_count = 0;

    for (const char* current = text;
         *current != '\0';
         ++current)
    {
        if (!font8x8::is_supported(*current))
        {
            return false;
        }

        if (character_count >= maximum_character_count)
        {
            return false;
        }

        ++character_count;
    }

    const std::int64_t text_left = x;
    const std::int64_t text_top = y;
    const std::int64_t text_width =
        character_count * advance;

    if (text_left >
        std::numeric_limits<std::int64_t>::max() - text_width)
    {
        return false;
    }

    const std::int64_t text_right_exclusive =
        text_left + text_width;
    const std::int64_t text_bottom_exclusive =
        text_top +
        static_cast<std::int64_t>(font8x8::glyph_height);

    if (text_right_exclusive <= 0 ||
        text_left >= screen_width ||
        text_bottom_exclusive <= 0 ||
        text_top >= screen_height)
    {
        return false;
    }

    bool drew_character = false;
    std::int64_t character_x = text_left;

    for (const char* current = text;
         *current != '\0';
         ++current)
    {
        const std::int64_t character_right_exclusive =
            character_x + advance;

        if (character_right_exclusive > 0 &&
            character_x < screen_width &&
            character_x >=
                std::numeric_limits<std::int32_t>::min() &&
            character_x <=
                std::numeric_limits<std::int32_t>::max())
        {
            if (draw_char(
                    static_cast<std::int32_t>(character_x),
                    y,
                    *current,
                    foreground_grayscale,
                    background_grayscale))
            {
                drew_character = true;
            }
        }

        character_x += advance;
    }

    return drew_character;
}


bool draw_bitmap_4bpp(
    std::int32_t x,
    std::int32_t y,
    const uint8_t* bitmap,
    std::size_t bitmap_size,
    std::size_t bitmap_width,
    std::size_t bitmap_height,
    Rotation rotation)
{
    if (bitmap == nullptr ||
        bitmap_width == 0 ||
        bitmap_height == 0)
    {
        return false;
    }

    std::size_t output_width = 0;
    std::size_t output_height = 0;

    switch (rotation)
    {
        case Rotation::Degrees0:
        case Rotation::Degrees180:
            output_width = bitmap_width;
            output_height = bitmap_height;
            break;

        case Rotation::Degrees90:
        case Rotation::Degrees270:
            output_width = bitmap_height;
            output_height = bitmap_width;
            break;

        default:
            return false;
    }

    if (bitmap_width >
        std::numeric_limits<std::size_t>::max() - 1)
    {
        return false;
    }

    const std::size_t source_row_bytes =
        (bitmap_width + 1) / 2;

    if (bitmap_height >
        std::numeric_limits<std::size_t>::max() /
            source_row_bytes)
    {
        return false;
    }

    const std::size_t required_size =
        source_row_bytes * bitmap_height;

    if (bitmap_size < required_size)
    {
        return false;
    }

    bool drew_pixel = false;

    for (std::int32_t screen_y = 0;
         screen_y < screen_height;
         ++screen_y)
    {
        const std::int64_t local_y_64 =
            static_cast<std::int64_t>(screen_y) -
            static_cast<std::int64_t>(y);

        if (local_y_64 < 0)
        {
            continue;
        }

        const std::size_t local_y =
            static_cast<std::size_t>(local_y_64);

        if (local_y >= output_height)
        {
            continue;
        }

        for (std::int32_t screen_x = 0;
             screen_x < screen_width;
             ++screen_x)
        {
            const std::int64_t local_x_64 =
                static_cast<std::int64_t>(screen_x) -
                static_cast<std::int64_t>(x);

            if (local_x_64 < 0)
            {
                continue;
            }

            const std::size_t local_x =
                static_cast<std::size_t>(local_x_64);

            if (local_x >= output_width)
            {
                continue;
            }

            std::size_t source_x = 0;
            std::size_t source_y = 0;

            switch (rotation)
            {
                case Rotation::Degrees0:
                    source_x = local_x;
                    source_y = local_y;
                    break;

                case Rotation::Degrees90:
                    source_x = local_y;
                    source_y = bitmap_height - 1 - local_x;
                    break;

                case Rotation::Degrees180:
                    source_x = bitmap_width - 1 - local_x;
                    source_y = bitmap_height - 1 - local_y;
                    break;

                case Rotation::Degrees270:
                    source_x = bitmap_width - 1 - local_y;
                    source_y = local_x;
                    break;

                default:
                    return false;
            }

            const uint8_t grayscale =
                read_4bpp_pixel(
                    bitmap,
                    source_row_bytes,
                    source_x,
                    source_y);

            if (framebuffer::set_pixel(
                    static_cast<std::size_t>(screen_x),
                    static_cast<std::size_t>(screen_y),
                    grayscale))
            {
                drew_pixel = true;
            }
        }
    }

    return drew_pixel;
}


bool draw_sprite_2bpp(
    std::int32_t x,
    std::int32_t y,
    const sprite::Sprite2bpp& image,
    Rotation rotation)
{
    if (image.data == nullptr ||
        image.width == 0 ||
        image.height == 0)
    {
        return false;
    }

    std::size_t output_width = 0;
    std::size_t output_height = 0;

    switch (rotation)
    {
        case Rotation::Degrees0:
        case Rotation::Degrees180:
            output_width = image.width;
            output_height = image.height;
            break;

        case Rotation::Degrees90:
        case Rotation::Degrees270:
            output_width = image.height;
            output_height = image.width;
            break;

        default:
            return false;
    }

    if (image.width >
        std::numeric_limits<std::size_t>::max() - 3)
    {
        return false;
    }

    const std::size_t source_row_bytes =
        (image.width + 3) / 4;

    if (image.height >
        std::numeric_limits<std::size_t>::max() /
            source_row_bytes)
    {
        return false;
    }

    const std::size_t required_size =
        source_row_bytes * image.height;

    if (image.data_size < required_size)
    {
        return false;
    }

    bool drew_pixel = false;

    for (std::int32_t screen_y = 0;
         screen_y < screen_height;
         ++screen_y)
    {
        const std::int64_t local_y_64 =
            static_cast<std::int64_t>(screen_y) -
            static_cast<std::int64_t>(y);

        if (local_y_64 < 0)
        {
            continue;
        }

        const std::size_t local_y =
            static_cast<std::size_t>(local_y_64);

        if (local_y >= output_height)
        {
            continue;
        }

        for (std::int32_t screen_x = 0;
             screen_x < screen_width;
             ++screen_x)
        {
            const std::int64_t local_x_64 =
                static_cast<std::int64_t>(screen_x) -
                static_cast<std::int64_t>(x);

            if (local_x_64 < 0)
            {
                continue;
            }

            const std::size_t local_x =
                static_cast<std::size_t>(local_x_64);

            if (local_x >= output_width)
            {
                continue;
            }

            std::size_t source_x = 0;
            std::size_t source_y = 0;

            switch (rotation)
            {
                case Rotation::Degrees0:
                    source_x = local_x;
                    source_y = local_y;
                    break;

                case Rotation::Degrees90:
                    source_x = local_y;
                    source_y = image.height - 1 - local_x;
                    break;

                case Rotation::Degrees180:
                    source_x = image.width - 1 - local_x;
                    source_y = image.height - 1 - local_y;
                    break;

                case Rotation::Degrees270:
                    source_x = image.width - 1 - local_y;
                    source_y = local_x;
                    break;

                default:
                    return false;
            }

            const uint8_t source_gray =
                read_2bpp_pixel(
                    image.data,
                    source_row_bytes,
                    source_x,
                    source_y);
            const uint8_t framebuffer_gray =
                static_cast<uint8_t>(
                    source_gray * 5U);

            if (framebuffer::set_pixel(
                    static_cast<std::size_t>(screen_x),
                    static_cast<std::size_t>(screen_y),
                    framebuffer_gray))
            {
                drew_pixel = true;
            }
        }
    }

    return drew_pixel;
}
}
