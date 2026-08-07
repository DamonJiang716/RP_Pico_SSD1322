#include "framebuffer.hpp"
#include "generated/test_arrow.hpp"
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


    bool draw_sprite_oled_test_frame()
    {
        framebuffer::clear(0);

        if (!graphics::draw_rectangle(
                0,
                0,
                static_cast<std::int32_t>(framebuffer::width),
                static_cast<std::int32_t>(framebuffer::height),
                15))
        {
            return false;
        }

        if (!graphics::draw_text(
                64,
                4,
                "SPRITE OLED TEST",
                15,
                0))
        {
            return false;
        }

        if (!graphics::draw_sprite_2bpp(
                16,
                18,
                generated_assets::test_arrow,
                graphics::Rotation::Degrees0))
        {
            return false;
        }

        if (!graphics::draw_sprite_2bpp(
                76,
                18,
                generated_assets::test_arrow,
                graphics::Rotation::Degrees90))
        {
            return false;
        }

        if (!graphics::draw_sprite_2bpp(
                136,
                18,
                generated_assets::test_arrow,
                graphics::Rotation::Degrees180))
        {
            return false;
        }

        if (!graphics::draw_sprite_2bpp(
                204,
                18,
                generated_assets::test_arrow,
                graphics::Rotation::Degrees270))
        {
            return false;
        }

        if (!graphics::draw_text(
                16,
                36,
                "R0",
                15,
                0))
        {
            return false;
        }

        if (!graphics::draw_text(
                72,
                36,
                "R90",
                15,
                0))
        {
            return false;
        }

        if (!graphics::draw_text(
                128,
                36,
                "R180",
                15,
                0))
        {
            return false;
        }

        if (!graphics::draw_text(
                196,
                36,
                "R270",
                15,
                0))
        {
            return false;
        }

        if (!graphics::fill_rectangle(
                16,
                50,
                40,
                10,
                0))
        {
            return false;
        }

        if (!graphics::fill_rectangle(
                76,
                50,
                40,
                10,
                5))
        {
            return false;
        }

        if (!graphics::fill_rectangle(
                136,
                50,
                40,
                10,
                10))
        {
            return false;
        }

        if (!graphics::fill_rectangle(
                200,
                50,
                40,
                10,
                15))
        {
            return false;
        }

        return true;
    }

    bool draw_cat()
    {
        framebuffer::clear(0);

        

        if (!graphics::draw_sprite_2bpp(0,0,generated_assets::test_arrow,graphics::Rotation::Degrees0))
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

    std::printf("Sprite OLED test starting\n");

    if (!ssd1322::initialise_display())
    {
        halt_with_message(
            "SSD1322 initialisation failed");
    }

   /*  if (!draw_sprite_oled_test_frame())
    {
        halt_with_message(
            "Sprite OLED frame drawing failed");
    }
 */
    if (!draw_cat())
        {
            halt_with_message(
                "Sprite OLED frame drawing failed");
        }
 
    if (!ssd1322::write_full_frame(
            framebuffer::data(),
            framebuffer::data_size()))
    {
        halt_with_message(
            "Sprite OLED frame transfer failed");
    }

    std::printf("Sprite OLED frame displayed\n");

    while (true)
    {
        sleep_ms(2000);

        std::printf("Sprite OLED test running\n");
    }
}
