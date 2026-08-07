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

constexpr uint button_pin = 22;

constexpr uint64_t debounce_time_us =
    30'000;

constexpr uint32_t button_poll_time_ms = 5;


void initialise_button()
{
    gpio_init(button_pin);
    gpio_set_dir(button_pin, GPIO_IN);

    // An external 2.7 kOhm pull-up resistor is fitted.
    // Do not enable an internal pull-up or pull-down.
    gpio_disable_pulls(button_pin);
}


[[noreturn]]
void halt_with_message(const char* message)
{
    std::printf("%s\n", message);

    while (true)
    {
        tight_loop_contents();
    }
}


bool draw_button_test_frame()
{
    framebuffer::clear(0x00);

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

    if (!graphics::fill_rectangle(
            4,
            3,
            248,
            12,
            0x03))
    {
        return false;
    }

    if (!graphics::draw_text(
            8,
            5,
            "DISPLAY TEST",
            0x0F,
            0x03))
    {
        return false;
    }

    if (!graphics::draw_text(
            8,
            22,
            "PRESS BUTTON",
            0x0F,
            0x00))
    {
        return false;
    }

    if (!graphics::draw_text(
            8,
            34,
            "TO TOGGLE OLED",
            0x0C,
            0x00))
    {
        return false;
    }

    if (!graphics::draw_text(
            8,
            50,
            "GP22 ACTIVE LOW",
            0x08,
            0x00))
    {
        return false;
    }

    return true;
}


void wait_for_stable_button_release()
{
    bool previous_raw_state =
        gpio_get(button_pin);
    uint64_t last_raw_change_us =
        time_us_64();

    while (true)
    {
        sleep_ms(button_poll_time_ms);

        const bool new_raw_state =
            gpio_get(button_pin);
        const uint64_t now_us =
            time_us_64();

        if (new_raw_state != previous_raw_state)
        {
            previous_raw_state =
                new_raw_state;
            last_raw_change_us =
                now_us;
        }

        if (new_raw_state &&
            now_us - last_raw_change_us >=
                debounce_time_us)
        {
            return;
        }
    }
}
}


int main()
{
    stdio_init_all();

    ssd1322::initialise_hardware();
    initialise_button();

    sleep_ms(1000);

    std::printf(
        "Display button test starting\n");

    std::printf(
        "Button input: GP22 active LOW\n");

    std::printf(
        "External pull-up: 2.7 kOhm to Pico 3V3\n");

    if (!ssd1322::initialise_display())
    {
        halt_with_message(
            "SSD1322 initialisation failed");
    }

    if (!draw_button_test_frame())
    {
        halt_with_message(
            "Button test frame drawing failed");
    }

    if (!ssd1322::write_full_frame(
            framebuffer::data(),
            framebuffer::data_size()))
    {
        halt_with_message(
            "Button test frame transfer failed");
    }

    std::printf(
        "Button test frame displayed\n");

    if (!gpio_get(button_pin))
    {
        std::printf(
            "Button is LOW at startup\n");
        std::printf(
            "Release button before testing\n");

        wait_for_stable_button_release();
    }

    std::printf("Button ready\n");

    bool display_enabled = true;
    bool previous_raw_state = true;
    bool stable_state = true;
    uint64_t last_raw_change_us =
        time_us_64();

    while (true)
    {
        const bool new_raw_state =
            gpio_get(button_pin);
        const uint64_t now_us =
            time_us_64();

        if (new_raw_state != previous_raw_state)
        {
            previous_raw_state =
                new_raw_state;
            last_raw_change_us =
                now_us;
        }

        if (new_raw_state != stable_state)
        {
            const uint64_t stable_duration_us =
                now_us - last_raw_change_us;

            if (stable_duration_us >=
                debounce_time_us)
            {
                const bool old_stable_state =
                    stable_state;

                stable_state =
                    new_raw_state;

                const bool pressed_event =
                    old_stable_state &&
                    !stable_state;

                if (pressed_event)
                {
                    display_enabled =
                        !display_enabled;

                    if (display_enabled)
                    {
                        ssd1322::ssd1322_display_on();

                        std::printf(
                            "Button pressed: OLED display ON\n");
                    }
                    else
                    {
                        ssd1322::ssd1322_display_off();

                        std::printf(
                            "Button pressed: OLED display OFF\n");
                    }
                }
            }
        }

        sleep_ms(button_poll_time_ms);
    }
}
