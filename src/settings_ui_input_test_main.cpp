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

constexpr uint UP_BUTTON_PIN = 2;
constexpr uint DOWN_BUTTON_PIN = 3;
constexpr uint ENTER_BUTTON_PIN = 4;

constexpr uint ROTARY_POSITION_1_PIN = 5;
constexpr uint ROTARY_POSITION_2_PIN = 6;
constexpr uint ROTARY_POSITION_3_PIN = 7;
constexpr uint ROTARY_POSITION_4_PIN = 8;
constexpr uint ROTARY_POSITION_5_PIN = 9;
constexpr uint ROTARY_POSITION_6_PIN = 10;
constexpr uint ROTARY_POSITION_7_PIN = 11;
constexpr uint ROTARY_POSITION_8_PIN = 12;

constexpr uint rotary_pins[] = {
    ROTARY_POSITION_1_PIN,
    ROTARY_POSITION_2_PIN,
    ROTARY_POSITION_3_PIN,
    ROTARY_POSITION_4_PIN,
    ROTARY_POSITION_5_PIN,
    ROTARY_POSITION_6_PIN,
    ROTARY_POSITION_7_PIN,
    ROTARY_POSITION_8_PIN
};

constexpr std::size_t rotary_position_count =
    sizeof(rotary_pins) / sizeof(rotary_pins[0]);

static_assert(rotary_position_count == 8);

constexpr std::uint64_t button_debounce_time_us =
    25'000;

constexpr std::uint64_t rotary_stable_time_us =
    30'000;

constexpr std::uint64_t heartbeat_interval_us =
    5'000'000;

constexpr std::uint32_t input_poll_time_ms = 5;

constexpr std::uint8_t black = 0x00;
constexpr std::uint8_t dim = 0x08;
constexpr std::uint8_t highlight = 0x0C;
constexpr std::uint8_t bright = 0x0F;

constexpr std::int32_t glyph_width = 8;
constexpr std::int32_t glyph_height = 8;

// Placeholder UI data; this is not a real battery ADC reading.
constexpr unsigned int demo_battery_level = 3;
constexpr bool demo_charging = false;

// This is placeholder UI data.
// It is not read from the RTC.
constexpr unsigned int demo_minute = 35;

enum class Mode
{
    OffCharge,
    Sample,
    Prime,
    Clean,
    Timer,
    Clock,
    Calibrate,
    Settings
};

constexpr Mode rotary_modes[] = {
    Mode::OffCharge,
    Mode::Sample,
    Mode::Prime,
    Mode::Clean,
    Mode::Timer,
    Mode::Clock,
    Mode::Calibrate,
    Mode::Settings
};

static_assert(
    sizeof(rotary_modes) /
        sizeof(rotary_modes[0]) ==
    rotary_position_count);

enum class SettingsItem
{
    AdjustClock,
    SamplingSpeed,
    DefaultVolumes,
    DisplaySettings
};

enum class SettingsView
{
    Menu,
    Detail
};

enum class RotaryReadingKind
{
    Gap,
    Unique,
    Multiple
};

struct ButtonState
{
    uint pin;
    const char* name;
    bool last_raw_state;
    bool stable_state;
    std::uint64_t raw_transition_time_us;
};

struct RotaryReading
{
    RotaryReadingKind kind;
    Mode mode;
};

struct RotaryState
{
    bool candidate_valid = false;
    Mode candidate = Mode::OffCharge;
    std::uint64_t candidate_since_us = 0;

    bool stable_valid = false;
    Mode stable = Mode::OffCharge;

    bool multiple_episode = false;
    bool multiple_warning_printed = false;
    std::uint64_t multiple_since_us = 0;
};

struct UiState
{
    bool stable_mode_valid = false;
    Mode stable_mode = Mode::OffCharge;
    SettingsItem selected_item =
        SettingsItem::AdjustClock;
    SettingsView settings_view =
        SettingsView::Menu;

    unsigned int demo_hour = 14;
    unsigned int sampling_speed = 50;
    unsigned int default_volume_ml = 500;
    unsigned int display_level = 10;
};

[[noreturn]]
void halt_with_message(const char* message)
{
    std::printf("%s\n", message);

    while (true)
    {
        tight_loop_contents();
    }
}

void initialise_input_pin(uint pin)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

void initialise_inputs()
{
    initialise_input_pin(UP_BUTTON_PIN);
    initialise_input_pin(DOWN_BUTTON_PIN);
    initialise_input_pin(ENTER_BUTTON_PIN);

    for (const uint pin : rotary_pins)
    {
        initialise_input_pin(pin);
    }
}

bool input_is_high(
    std::uint32_t gpio_snapshot,
    uint pin)
{
    return (gpio_snapshot & (1u << pin)) != 0;
}

ButtonState make_button_state(
    uint pin,
    const char* name,
    std::uint32_t gpio_snapshot,
    std::uint64_t now_us)
{
    const bool raw_state =
        input_is_high(gpio_snapshot, pin);

    return {
        pin,
        name,
        raw_state,
        raw_state,
        now_us
    };
}

bool update_button(
    ButtonState& state,
    bool raw_state,
    std::uint64_t now_us)
{
    if (raw_state != state.last_raw_state)
    {
        state.last_raw_state = raw_state;
        state.raw_transition_time_us = now_us;
    }

    if (raw_state == state.stable_state ||
        now_us - state.raw_transition_time_us <
            button_debounce_time_us)
    {
        return false;
    }

    const bool old_stable_state =
        state.stable_state;

    state.stable_state = raw_state;

    return old_stable_state &&
           !state.stable_state;
}

RotaryReading read_rotary(
    std::uint32_t gpio_snapshot)
{
    std::size_t low_count = 0;
    std::size_t low_position = 0;

    for (std::size_t index = 0;
         index < rotary_position_count;
         ++index)
    {
        if (!input_is_high(
                gpio_snapshot,
                rotary_pins[index]))
        {
            ++low_count;
            low_position = index;
        }
    }

    if (low_count == 0)
    {
        return {
            RotaryReadingKind::Gap,
            Mode::OffCharge
        };
    }

    if (low_count == 1)
    {
        return {
            RotaryReadingKind::Unique,
            rotary_modes[low_position]
        };
    }

    return {
        RotaryReadingKind::Multiple,
        Mode::OffCharge
    };
}

bool update_rotary(
    RotaryState& state,
    const RotaryReading& reading,
    std::uint64_t now_us,
    Mode& accepted_mode)
{
    if (reading.kind ==
        RotaryReadingKind::Gap)
    {
        state.candidate_valid = false;
        state.multiple_episode = false;
        state.multiple_warning_printed = false;
        return false;
    }

    if (reading.kind ==
        RotaryReadingKind::Multiple)
    {
        state.candidate_valid = false;

        if (!state.multiple_episode)
        {
            state.multiple_episode = true;
            state.multiple_warning_printed = false;
            state.multiple_since_us = now_us;
        }
        else if (!state.multiple_warning_printed &&
                 now_us - state.multiple_since_us >=
                     rotary_stable_time_us)
        {
            std::printf(
                "Warning: invalid rotary state\n");

            state.multiple_warning_printed = true;
        }

        return false;
    }

    state.multiple_episode = false;
    state.multiple_warning_printed = false;

    if (!state.candidate_valid ||
        reading.mode != state.candidate)
    {
        state.candidate_valid = true;
        state.candidate = reading.mode;
        state.candidate_since_us = now_us;
        return false;
    }

    if (now_us - state.candidate_since_us <
        rotary_stable_time_us)
    {
        return false;
    }

    if (state.stable_valid &&
        state.stable == state.candidate)
    {
        return false;
    }

    state.stable_valid = true;
    state.stable = state.candidate;
    accepted_mode = state.stable;
    return true;
}

const char* mode_name(Mode mode)
{
    switch (mode)
    {
    case Mode::OffCharge:
        return "OFF/CHARGE";
    case Mode::Sample:
        return "SAMPLE";
    case Mode::Prime:
        return "PRIME";
    case Mode::Clean:
        return "CLEAN";
    case Mode::Timer:
        return "TIMER";
    case Mode::Clock:
        return "CLOCK";
    case Mode::Calibrate:
        return "CALIBRATE";
    case Mode::Settings:
        return "SETTINGS";
    }

    return "UNKNOWN";
}

const char* mode_text(Mode mode)
{
    switch (mode)
    {
    case Mode::OffCharge:
        return "OFF CHARGE";
    case Mode::Sample:
        return "SAMPLE";
    case Mode::Prime:
        return "PRIME";
    case Mode::Clean:
        return "CLEAN";
    case Mode::Timer:
        return "TIMER";
    case Mode::Clock:
        return "CLOCK";
    case Mode::Calibrate:
        return "CALIBRATE";
    case Mode::Settings:
        return "SETTINGS";
    }

    return "UNKNOWN";
}

const char* settings_item_name(
    SettingsItem item)
{
    switch (item)
    {
    case SettingsItem::AdjustClock:
        return "Adjust clock";
    case SettingsItem::SamplingSpeed:
        return "Sampling speed";
    case SettingsItem::DefaultVolumes:
        return "Default volumes";
    case SettingsItem::DisplaySettings:
        return "Display settings";
    }

    return "Unknown";
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

bool draw_centered_text(
    std::int32_t y,
    const char* text,
    std::uint8_t foreground,
    std::uint8_t background)
{
    const std::size_t length =
        text_length(text);

    if (length >
        framebuffer::width /
            static_cast<std::size_t>(glyph_width))
    {
        return false;
    }

    const std::size_t pixel_width =
        length *
        static_cast<std::size_t>(glyph_width);

    const std::int32_t x =
        static_cast<std::int32_t>(
            (framebuffer::width - pixel_width) / 2);

    return graphics::draw_text(
        x,
        y,
        text,
        foreground,
        background);
}

bool draw_colon_cell(
    std::int32_t x,
    std::int32_t y,
    std::uint8_t foreground,
    std::uint8_t background)
{
    if (!graphics::fill_rectangle(
            x,
            y,
            glyph_width,
            glyph_height,
            background))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            x + 3,
            y + 1,
            2,
            2,
            foreground))
    {
        return false;
    }

    return graphics::fill_rectangle(
        x + 3,
        y + 5,
        2,
        2,
        foreground);
}

bool draw_percent_cell(
    std::int32_t x,
    std::int32_t y,
    std::uint8_t foreground,
    std::uint8_t background)
{
    if (!graphics::fill_rectangle(
            x,
            y,
            glyph_width,
            glyph_height,
            background))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            x + 1,
            y + 1,
            2,
            2,
            foreground))
    {
        return false;
    }

    if (!graphics::draw_line(
            x + 6,
            y + 1,
            x + 1,
            y + 6,
            foreground))
    {
        return false;
    }

    return graphics::fill_rectangle(
        x + 5,
        y + 5,
        2,
        2,
        foreground);
}

bool draw_slash_cell(
    std::int32_t x,
    std::int32_t y,
    std::uint8_t foreground,
    std::uint8_t background)
{
    if (!graphics::fill_rectangle(
            x,
            y,
            glyph_width,
            glyph_height,
            background))
    {
        return false;
    }

    return graphics::draw_line(
        x + 6,
        y,
        x + 1,
        y + 7,
        foreground);
}

bool draw_mock_time(
    std::int32_t x,
    std::int32_t y,
    unsigned int hour,
    std::uint8_t foreground,
    std::uint8_t background)
{
    char time_text[6];

    const int written = std::snprintf(
        time_text,
        sizeof(time_text),
        "%02u %02u",
        hour,
        demo_minute);

    if (written < 0 ||
        static_cast<std::size_t>(written) >=
            sizeof(time_text))
    {
        return false;
    }

    if (!graphics::draw_text(
            x,
            y,
            time_text,
            foreground,
            background))
    {
        return false;
    }

    return draw_colon_cell(
        x + 2 * glyph_width,
        y,
        foreground,
        background);
}

bool draw_mode_text(
    Mode mode,
    std::int32_t y,
    std::uint8_t foreground,
    std::uint8_t background)
{
    if (mode != Mode::OffCharge)
    {
        return draw_centered_text(
            y,
            mode_text(mode),
            foreground,
            background);
    }

    constexpr std::int32_t off_charge_x = 88;

    if (!graphics::draw_text(
            off_charge_x,
            y,
            "OFF",
            foreground,
            background))
    {
        return false;
    }

    if (!draw_slash_cell(
            off_charge_x + 3 * glyph_width,
            y,
            foreground,
            background))
    {
        return false;
    }

    return graphics::draw_text(
        off_charge_x + 4 * glyph_width,
        y,
        "CHARGE",
        foreground,
        background);
}

bool render_battery_icon()
{
    static_assert(demo_battery_level <= 4);

    if (!graphics::draw_rectangle(
            4,
            2,
            27,
            9,
            bright))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            31,
            4,
            3,
            5,
            bright))
    {
        return false;
    }

    for (unsigned int index = 0;
         index < 4;
         ++index)
    {
        const std::uint8_t level =
            index < demo_battery_level ?
                bright : black;

        if (!graphics::fill_rectangle(
                6 +
                    static_cast<std::int32_t>(index) * 6,
                4,
                4,
                5,
                level))
        {
            return false;
        }
    }

    if constexpr (demo_charging)
    {
        if (!graphics::draw_line(
                17,
                3,
                14,
                6,
                black))
        {
            return false;
        }

        if (!graphics::draw_line(
                14,
                6,
                18,
                6,
                black))
        {
            return false;
        }

        if (!graphics::draw_line(
                18,
                6,
                15,
                9,
                black))
        {
            return false;
        }
    }

    return true;
}

bool render_status_bar(const UiState& ui)
{
    if (!render_battery_icon())
    {
        return false;
    }

    if (!draw_mode_text(
            ui.stable_mode,
            2,
            bright,
            black))
    {
        return false;
    }

    if (!draw_mock_time(
            216,
            2,
            ui.demo_hour,
            bright,
            black))
    {
        return false;
    }

    return graphics::draw_horizontal_line(
        0,
        12,
        static_cast<std::int32_t>(
            framebuffer::width),
        dim);
}

bool render_placeholder_page(const UiState& ui)
{
    // TODO:
    // Replace with final Medusa mode page.
    return draw_mode_text(
        ui.stable_mode,
        34,
        bright,
        black);
}

bool draw_percentage_right(
    unsigned int percentage,
    std::int32_t y,
    std::uint8_t foreground,
    std::uint8_t background)
{
    char digits[4];

    const int written = std::snprintf(
        digits,
        sizeof(digits),
        "%u",
        percentage);

    if (written <= 0 ||
        static_cast<std::size_t>(written) >=
            sizeof(digits))
    {
        return false;
    }

    const std::int32_t digit_width =
        written * glyph_width;

    constexpr std::int32_t right_edge = 248;

    const std::int32_t x =
        right_edge - digit_width - glyph_width;

    if (!graphics::draw_text(
            x,
            y,
            digits,
            foreground,
            background))
    {
        return false;
    }

    return draw_percent_cell(
        x + digit_width,
        y,
        foreground,
        background);
}

bool draw_volume_right(
    unsigned int volume_ml,
    std::int32_t y,
    std::uint8_t foreground,
    std::uint8_t background)
{
    char volume_text[7];

    const int written = std::snprintf(
        volume_text,
        sizeof(volume_text),
        "%umL",
        volume_ml);

    if (written <= 0 ||
        static_cast<std::size_t>(written) >=
            sizeof(volume_text))
    {
        return false;
    }

    constexpr std::int32_t right_edge = 248;

    const std::int32_t x =
        right_edge - written * glyph_width;

    return graphics::draw_text(
        x,
        y,
        volume_text,
        foreground,
        background);
}

bool render_settings_menu_row(
    const UiState& ui,
    SettingsItem item,
    std::int32_t row_y)
{
    const bool selected =
        ui.selected_item == item;

    const std::uint8_t background =
        selected ? highlight : black;

    const std::uint8_t foreground =
        selected ? black : bright;

    if (!graphics::fill_rectangle(
            4,
            row_y,
            248,
            10,
            background))
    {
        return false;
    }

    if (!graphics::draw_text(
            8,
            row_y + 1,
            settings_item_name(item),
            foreground,
            background))
    {
        return false;
    }

    if (item == SettingsItem::SamplingSpeed)
    {
        return draw_percentage_right(
            ui.sampling_speed,
            row_y + 1,
            foreground,
            background);
    }

    if (item == SettingsItem::DefaultVolumes)
    {
        return draw_volume_right(
            ui.default_volume_ml,
            row_y + 1,
            foreground,
            background);
    }

    return true;
}

bool render_settings_menu(const UiState& ui)
{
    if (!render_settings_menu_row(
            ui,
            SettingsItem::AdjustClock,
            15))
    {
        return false;
    }

    if (!render_settings_menu_row(
            ui,
            SettingsItem::SamplingSpeed,
            25))
    {
        return false;
    }

    if (!render_settings_menu_row(
            ui,
            SettingsItem::DefaultVolumes,
            35))
    {
        return false;
    }

    return render_settings_menu_row(
        ui,
        SettingsItem::DisplaySettings,
        45);
}

bool draw_centered_percentage(
    unsigned int percentage,
    std::int32_t y,
    std::uint8_t foreground,
    std::uint8_t background)
{
    char digits[4];

    const int written = std::snprintf(
        digits,
        sizeof(digits),
        "%u",
        percentage);

    if (written <= 0 ||
        static_cast<std::size_t>(written) >=
            sizeof(digits))
    {
        return false;
    }

    const std::int32_t total_width =
        (written + 1) * glyph_width;

    const std::int32_t x =
        (static_cast<std::int32_t>(
             framebuffer::width) -
         total_width) /
        2;

    if (!graphics::draw_text(
            x,
            y,
            digits,
            foreground,
            background))
    {
        return false;
    }

    return draw_percent_cell(
        x + written * glyph_width,
        y,
        foreground,
        background);
}

bool render_settings_detail(const UiState& ui)
{
    switch (ui.selected_item)
    {
    case SettingsItem::AdjustClock:
        if (!draw_centered_text(
                20,
                "ADJUST CLOCK",
                bright,
                black))
        {
            return false;
        }

        return draw_mock_time(
            108,
            38,
            ui.demo_hour,
            bright,
            black);

    case SettingsItem::SamplingSpeed:
        if (!draw_centered_text(
                20,
                "SAMPLING SPEED",
                bright,
                black))
        {
            return false;
        }

        return draw_centered_percentage(
            ui.sampling_speed,
            38,
            bright,
            black);

    case SettingsItem::DefaultVolumes:
    {
        if (!draw_centered_text(
                20,
                "DEFAULT VOLUME",
                bright,
                black))
        {
            return false;
        }

        char volume_text[8];

        const int written = std::snprintf(
            volume_text,
            sizeof(volume_text),
            "%u mL",
            ui.default_volume_ml);

        if (written <= 0 ||
            static_cast<std::size_t>(written) >=
                sizeof(volume_text))
        {
            return false;
        }

        return draw_centered_text(
            38,
            volume_text,
            bright,
            black);
    }

    case SettingsItem::DisplaySettings:
    {
        if (!draw_centered_text(
                20,
                "DISPLAY",
                bright,
                black))
        {
            return false;
        }

        char level_text[9];

        const int written = std::snprintf(
            level_text,
            sizeof(level_text),
            "LEVEL %u",
            ui.display_level);

        if (written <= 0 ||
            static_cast<std::size_t>(written) >=
                sizeof(level_text))
        {
            return false;
        }

        return draw_centered_text(
            38,
            level_text,
            bright,
            black);
    }
    }

    return false;
}

bool render_current_page(const UiState& ui)
{
    if (!render_status_bar(ui))
    {
        return false;
    }

    if (ui.stable_mode != Mode::Settings)
    {
        return render_placeholder_page(ui);
    }

    if (ui.settings_view == SettingsView::Menu)
    {
        return render_settings_menu(ui);
    }

    return render_settings_detail(ui);
}

void refresh_display(const UiState& ui)
{
    framebuffer::clear(black);

    if (!render_current_page(ui))
    {
        halt_with_message(
            "Settings UI framebuffer drawing failed");
    }

    if (!ssd1322::write_full_frame(
            framebuffer::data(),
            framebuffer::data_size()))
    {
        halt_with_message(
            "Settings UI framebuffer transfer failed");
    }
}

SettingsItem previous_settings_item(
    SettingsItem item)
{
    switch (item)
    {
    case SettingsItem::AdjustClock:
        return SettingsItem::DisplaySettings;
    case SettingsItem::SamplingSpeed:
        return SettingsItem::AdjustClock;
    case SettingsItem::DefaultVolumes:
        return SettingsItem::SamplingSpeed;
    case SettingsItem::DisplaySettings:
        return SettingsItem::DefaultVolumes;
    }

    return SettingsItem::AdjustClock;
}

SettingsItem next_settings_item(
    SettingsItem item)
{
    switch (item)
    {
    case SettingsItem::AdjustClock:
        return SettingsItem::SamplingSpeed;
    case SettingsItem::SamplingSpeed:
        return SettingsItem::DefaultVolumes;
    case SettingsItem::DefaultVolumes:
        return SettingsItem::DisplaySettings;
    case SettingsItem::DisplaySettings:
        return SettingsItem::AdjustClock;
    }

    return SettingsItem::AdjustClock;
}

bool increase_detail_value(UiState& ui)
{
    switch (ui.selected_item)
    {
    case SettingsItem::AdjustClock:
        if (ui.demo_hour >= 23)
        {
            return false;
        }

        ++ui.demo_hour;
        std::printf(
            "Adjust clock: %02u:%02u\n",
            ui.demo_hour,
            demo_minute);
        return true;

    case SettingsItem::SamplingSpeed:
        if (ui.sampling_speed >= 100)
        {
            return false;
        }

        ui.sampling_speed += 5;
        std::printf(
            "Sampling speed: %u%%\n",
            ui.sampling_speed);
        return true;

    case SettingsItem::DefaultVolumes:
        if (ui.default_volume_ml >= 2000)
        {
            return false;
        }

        ui.default_volume_ml += 50;
        std::printf(
            "Default volume: %u mL\n",
            ui.default_volume_ml);
        return true;

    case SettingsItem::DisplaySettings:
        if (ui.display_level >= 15)
        {
            return false;
        }

        ++ui.display_level;
        std::printf(
            "Display level: %u\n",
            ui.display_level);
        return true;
    }

    return false;
}

bool decrease_detail_value(UiState& ui)
{
    switch (ui.selected_item)
    {
    case SettingsItem::AdjustClock:
        if (ui.demo_hour == 0)
        {
            return false;
        }

        --ui.demo_hour;
        std::printf(
            "Adjust clock: %02u:%02u\n",
            ui.demo_hour,
            demo_minute);
        return true;

    case SettingsItem::SamplingSpeed:
        if (ui.sampling_speed == 0)
        {
            return false;
        }

        ui.sampling_speed -= 5;
        std::printf(
            "Sampling speed: %u%%\n",
            ui.sampling_speed);
        return true;

    case SettingsItem::DefaultVolumes:
        if (ui.default_volume_ml <= 50)
        {
            return false;
        }

        ui.default_volume_ml -= 50;
        std::printf(
            "Default volume: %u mL\n",
            ui.default_volume_ml);
        return true;

    case SettingsItem::DisplaySettings:
        if (ui.display_level == 0)
        {
            return false;
        }

        --ui.display_level;
        std::printf(
            "Display level: %u\n",
            ui.display_level);
        return true;
    }

    return false;
}

bool handle_up_press(UiState& ui)
{
    if (!ui.stable_mode_valid ||
        ui.stable_mode != Mode::Settings)
    {
        return false;
    }

    if (ui.settings_view == SettingsView::Detail)
    {
        return increase_detail_value(ui);
    }

    ui.selected_item =
        previous_settings_item(
            ui.selected_item);

    std::printf(
        "Settings selection: %s\n",
        settings_item_name(
            ui.selected_item));

    return true;
}

bool handle_down_press(UiState& ui)
{
    if (!ui.stable_mode_valid ||
        ui.stable_mode != Mode::Settings)
    {
        return false;
    }

    if (ui.settings_view == SettingsView::Detail)
    {
        return decrease_detail_value(ui);
    }

    ui.selected_item =
        next_settings_item(
            ui.selected_item);

    std::printf(
        "Settings selection: %s\n",
        settings_item_name(
            ui.selected_item));

    return true;
}

bool handle_enter_press(UiState& ui)
{
    if (!ui.stable_mode_valid ||
        ui.stable_mode != Mode::Settings)
    {
        return false;
    }

    if (ui.settings_view == SettingsView::Menu)
    {
        ui.settings_view = SettingsView::Detail;

        std::printf(
            "Opened setting: %s\n",
            settings_item_name(
                ui.selected_item));
    }
    else
    {
        ui.settings_view = SettingsView::Menu;

        std::printf(
            "Returned to Settings menu\n");
    }

    return true;
}
}

int main()
{
    stdio_init_all();

    ssd1322::initialise_hardware();
    initialise_inputs();

    sleep_ms(1000);

    std::printf(
        "=== MEDUSA SETTINGS UI INPUT TEST ===\n");
    std::printf("UP pin: GP2\n");
    std::printf("DOWN pin: GP3\n");
    std::printf("ENTER pin: GP4\n");
    std::printf("Rotary pins: GP5-GP12\n");
    std::printf(
        "All inputs use RP2040 internal pull-ups\n");
    std::printf("Buttons are active-low\n");

    if (!ssd1322::initialise_display())
    {
        halt_with_message(
            "SSD1322 initialisation failed");
    }

    std::printf("Display initialised\n");

    const std::uint32_t initial_gpio_snapshot =
        gpio_get_all();

    const std::uint64_t initial_time_us =
        time_us_64();

    ButtonState up_button =
        make_button_state(
            UP_BUTTON_PIN,
            "UP",
            initial_gpio_snapshot,
            initial_time_us);

    ButtonState down_button =
        make_button_state(
            DOWN_BUTTON_PIN,
            "DOWN",
            initial_gpio_snapshot,
            initial_time_us);

    ButtonState enter_button =
        make_button_state(
            ENTER_BUTTON_PIN,
            "ENTER",
            initial_gpio_snapshot,
            initial_time_us);

    RotaryState rotary_state;
    UiState ui;

    std::uint64_t last_heartbeat_us =
        initial_time_us;

    while (true)
    {
        const std::uint64_t now_us =
            time_us_64();

        const std::uint32_t gpio_snapshot =
            gpio_get_all();

        bool ui_changed = false;

        Mode accepted_mode = Mode::OffCharge;

        if (update_rotary(
                rotary_state,
                read_rotary(gpio_snapshot),
                now_us,
                accepted_mode))
        {
            ui.stable_mode_valid = true;
            ui.stable_mode = accepted_mode;
            ui.settings_view = SettingsView::Menu;
            ui_changed = true;

            std::printf(
                "Mode changed: %s\n",
                mode_name(accepted_mode));
        }

        const bool up_pressed =
            update_button(
                up_button,
                input_is_high(
                    gpio_snapshot,
                    up_button.pin),
                now_us);

        const bool down_pressed =
            update_button(
                down_button,
                input_is_high(
                    gpio_snapshot,
                    down_button.pin),
                now_us);

        const bool enter_pressed =
            update_button(
                enter_button,
                input_is_high(
                    gpio_snapshot,
                    enter_button.pin),
                now_us);

        if (up_pressed)
        {
            std::printf(
                "%s pressed\n",
                up_button.name);

            ui_changed =
                handle_up_press(ui) ||
                ui_changed;
        }

        if (down_pressed)
        {
            std::printf(
                "%s pressed\n",
                down_button.name);

            ui_changed =
                handle_down_press(ui) ||
                ui_changed;
        }

        if (enter_pressed)
        {
            std::printf(
                "%s pressed\n",
                enter_button.name);

            ui_changed =
                handle_enter_press(ui) ||
                ui_changed;
        }

        if (ui_changed &&
            ui.stable_mode_valid)
        {
            refresh_display(ui);
        }

        if (now_us - last_heartbeat_us >=
            heartbeat_interval_us)
        {
            std::printf(
                "Settings UI input test running\n");

            last_heartbeat_us = now_us;
        }

        sleep_ms(input_poll_time_ms);
    }
}
