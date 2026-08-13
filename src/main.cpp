#include "font8x8.hpp"
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
constexpr std::size_t pump_count = 4;
constexpr std::size_t log_count = 3;

static_assert(rotary_position_count == 8);
static_assert(pump_count == 4);

constexpr std::uint64_t button_debounce_time_us =
    25'000;
constexpr std::uint64_t rotary_stable_time_us =
    30'000;
constexpr std::uint32_t input_poll_time_ms = 5;

constexpr std::uint64_t calibration_run_time_us =
    2'000'000;
constexpr std::uint64_t sample_delay_demo_time_us =
    3'000'000;
constexpr std::uint64_t pump_update_interval_us =
    250'000;
constexpr std::uint64_t flush_update_interval_us =
    500'000;

constexpr std::uint8_t black = 0x00;
constexpr std::uint8_t dim = 0x08;
constexpr std::uint8_t highlight = 0x0C;
constexpr std::uint8_t bright = 0x0F;

constexpr std::int32_t glyph_width = 8;
constexpr std::int32_t glyph_height = 8;

// Prototype mock only. This is not a battery ADC reading or a real
// charger-status input.
constexpr unsigned int demo_battery_level = 3;
constexpr bool demo_charging = false;

// Prototype mock only. These values are not read from an RTC.
constexpr unsigned int demo_day = 13;
constexpr const char* demo_month = "AUG";
constexpr unsigned int demo_year = 2026;

enum class Mode
{
    Settings,
    Calibrate,
    Prime,
    Sample,
    Flush,
    Log,
    Clock
};

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
    AdjustClock,
    SamplingSpeed,
    DefaultVolumes,
    DisplaySettings
};

enum class CalibrateState
{
    SelectPump,
    Ready,
    Running,
    EnterVolume,
    Result
};

enum class PrimeState
{
    Setup,
    Running,
    Complete,
    Stopped
};

enum class SampleState
{
    Setup,
    StartType,
    DelayWaiting,
    Ready,
    Running,
    Complete,
    Stopped
};

enum class SampleStartType
{
    Immediate,
    Delay30Minutes,
    Delay1Hour
};

enum class FlushState
{
    Setup,
    Running,
    Complete,
    Stopped
};

enum class LogView
{
    List,
    Detail
};

enum class RotaryReadingKind
{
    Gap,
    Unique,
    Multiple
};

enum class RotaryEventKind
{
    None,
    ModeSelected,
    Reserved
};

struct ButtonState
{
    uint pin;
    const char* name;
    bool last_raw_state;
    bool stable_state;
    std::uint64_t raw_transition_time_us;
};

struct InputEvents
{
    bool up_pressed = false;
    bool down_pressed = false;
    bool enter_pressed = false;
};

struct RotaryReading
{
    RotaryReadingKind kind;
    std::size_t position;
};

struct RotaryEvent
{
    RotaryEventKind kind = RotaryEventKind::None;
    Mode mode = Mode::Settings;
};

struct RotaryState
{
    bool candidate_valid = false;
    std::size_t candidate_position = 0;
    std::uint64_t candidate_since_us = 0;

    bool stable_position_valid = false;
    std::size_t stable_position = 0;

    bool multiple_episode = false;
    bool multiple_warning_printed = false;
    std::uint64_t multiple_since_us = 0;
};

struct SettingsData
{
    SettingsItem selected_item =
        SettingsItem::AdjustClock;
    SettingsView view = SettingsView::Menu;
    std::size_t edit_field = 0;

    unsigned int demo_hour = 14;
    unsigned int demo_minute = 35;

    unsigned int pump_speed[pump_count] = {
        50, 50, 50, 50
    };

    unsigned int default_volume_ml[pump_count] = {
        500, 500, 500, 500
    };

    unsigned int display_level = 10;
};

struct CalibrateData
{
    CalibrateState state =
        CalibrateState::SelectPump;
    std::size_t selected_pump = 0;
    unsigned int measured_volume_ml = 50;

    // Prototype mock motor data. No real motor or AVR is queried.
    unsigned int mock_rotations = 100;
    unsigned int coefficient_milli = 500;
    std::uint64_t state_started_us = 0;
};

struct PrimeData
{
    PrimeState state = PrimeState::Setup;
    std::size_t edit_pump = 0;
    unsigned int target_volume_ml[pump_count] = {};

    // Prototype mock current volumes. Future values will come from
    // AVR-to-RP2040 UART messages.
    unsigned int current_volume_ml[pump_count] = {};
    std::uint64_t last_update_us = 0;
    std::uint64_t serial_tick_count = 0;
};

struct SampleData
{
    SampleState state = SampleState::Setup;
    SampleStartType start_type =
        SampleStartType::Immediate;
    std::size_t edit_pump = 0;
    unsigned int target_volume_ml[pump_count] = {};

    // Prototype mock current volumes. Future values will come from
    // AVR-to-RP2040 UART messages.
    unsigned int current_volume_ml[pump_count] = {};
    std::uint64_t state_started_us = 0;
    std::uint64_t last_update_us = 0;
    std::uint64_t serial_tick_count = 0;
};

struct FlushData
{
    FlushState state = FlushState::Setup;
    unsigned int duration_minutes = 2;
    unsigned int remaining_seconds = 120;
    std::uint64_t last_update_us = 0;
};

struct LogData
{
    LogView view = LogView::List;
    std::size_t selected_log = 0;
};

struct UiState
{
    bool current_mode_valid = false;
    Mode current_mode = Mode::Settings;

    SettingsData settings;
    CalibrateData calibrate;
    PrimeData prime;
    SampleData sample;
    FlushData flush;
    LogData log;
};

struct SampleLog
{
    unsigned int day;
    const char* month;
    unsigned int hour;
    unsigned int minute;
    unsigned int volume_ml[pump_count];
    unsigned int duration_seconds;
    unsigned int battery_decivolts;
};

// Prototype mock logs. Nothing is read from or written to flash,
// EEPROM, an SD card, or a filesystem.
constexpr SampleLog sample_logs[log_count] = {
    {12, "AUG", 14, 35, {500, 501, 499, 500}, 102, 131},
    {11, "AUG", 9, 20, {480, 482, 479, 481}, 96, 132},
    {10, "AUG", 16, 42, {525, 523, 526, 524}, 108, 130}
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

InputEvents read_buttons(
    ButtonState& up_button,
    ButtonState& down_button,
    ButtonState& enter_button,
    std::uint32_t gpio_snapshot,
    std::uint64_t now_us)
{
    InputEvents events;

    events.up_pressed = update_button(
        up_button,
        input_is_high(
            gpio_snapshot,
            up_button.pin),
        now_us);

    events.down_pressed = update_button(
        down_button,
        input_is_high(
            gpio_snapshot,
            down_button.pin),
        now_us);

    events.enter_pressed = update_button(
        enter_button,
        input_is_high(
            gpio_snapshot,
            enter_button.pin),
        now_us);

    if (events.up_pressed)
    {
        std::printf("UP pressed\n");
    }

    if (events.down_pressed)
    {
        std::printf("DOWN pressed\n");
    }

    if (events.enter_pressed)
    {
        std::printf("ENTER pressed\n");
    }

    return events;
}

RotaryReading read_rotary(
    std::uint32_t gpio_snapshot)
{
    std::size_t low_count = 0;
    std::size_t low_position = 0;

    for (std::size_t position = 0;
         position < rotary_position_count;
         ++position)
    {
        if (!input_is_high(
                gpio_snapshot,
                rotary_pins[position]))
        {
            ++low_count;
            low_position = position;
        }
    }

    if (low_count == 0)
    {
        return {
            RotaryReadingKind::Gap,
            0
        };
    }

    if (low_count == 1)
    {
        return {
            RotaryReadingKind::Unique,
            low_position
        };
    }

    return {
        RotaryReadingKind::Multiple,
        0
    };
}

Mode mode_for_rotary_position(
    std::size_t position)
{
    switch (position)
    {
    case 0:
        return Mode::Settings;
    case 1:
        return Mode::Calibrate;
    case 2:
        return Mode::Prime;
    case 3:
        return Mode::Sample;
    case 4:
        return Mode::Flush;
    case 5:
        return Mode::Log;
    case 6:
        return Mode::Clock;
    default:
        return Mode::Settings;
    }
}

RotaryEvent update_rotary(
    RotaryState& state,
    const RotaryReading& reading,
    std::uint64_t now_us)
{
    if (reading.kind ==
        RotaryReadingKind::Gap)
    {
        state.candidate_valid = false;
        state.multiple_episode = false;
        state.multiple_warning_printed = false;
        return {};
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

        return {};
    }

    state.multiple_episode = false;
    state.multiple_warning_printed = false;

    if (!state.candidate_valid ||
        state.candidate_position !=
            reading.position)
    {
        state.candidate_valid = true;
        state.candidate_position =
            reading.position;
        state.candidate_since_us = now_us;
        return {};
    }

    if (now_us - state.candidate_since_us <
        rotary_stable_time_us)
    {
        return {};
    }

    if (state.stable_position_valid &&
        state.stable_position ==
            state.candidate_position)
    {
        return {};
    }

    state.stable_position_valid = true;
    state.stable_position =
        state.candidate_position;

    if (state.stable_position == 7)
    {
        std::printf(
            "Rotary position 8 reserved\n");

        return {
            RotaryEventKind::Reserved,
            Mode::Settings
        };
    }

    return {
        RotaryEventKind::ModeSelected,
        mode_for_rotary_position(
            state.stable_position)
    };
}

const char* mode_name(Mode mode)
{
    switch (mode)
    {
    case Mode::Settings:
        return "SETTINGS";
    case Mode::Calibrate:
        return "CALIBRATE";
    case Mode::Prime:
        return "PRIME";
    case Mode::Sample:
        return "SAMPLE";
    case Mode::Flush:
        return "FLUSH";
    case Mode::Log:
        return "LOG";
    case Mode::Clock:
        return "CLOCK";
    }

    return "UNKNOWN";
}

const char* mode_header_text(Mode mode)
{
    if (mode == Mode::Calibrate)
    {
        return "CALIB";
    }

    return mode_name(mode);
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

void clear_pump_values(
    unsigned int values[pump_count])
{
    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        values[pump] = 0;
    }
}

void copy_default_volumes(
    unsigned int destination[pump_count],
    const SettingsData& settings)
{
    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        destination[pump] =
            settings.default_volume_ml[pump];
    }
}

void leave_current_mode(UiState& ui)
{
    if (!ui.current_mode_valid)
    {
        return;
    }

    switch (ui.current_mode)
    {
    case Mode::Settings:
        ui.settings.view = SettingsView::Menu;
        ui.settings.edit_field = 0;
        break;
    case Mode::Calibrate:
        ui.calibrate.state =
            CalibrateState::SelectPump;
        ui.calibrate.state_started_us = 0;
        break;
    case Mode::Prime:
        ui.prime.state = PrimeState::Setup;
        ui.prime.last_update_us = 0;
        break;
    case Mode::Sample:
        ui.sample.state = SampleState::Setup;
        ui.sample.state_started_us = 0;
        ui.sample.last_update_us = 0;
        break;
    case Mode::Flush:
        ui.flush.state = FlushState::Setup;
        ui.flush.last_update_us = 0;
        break;
    case Mode::Log:
        ui.log.view = LogView::List;
        break;
    case Mode::Clock:
        break;
    }
}

void enter_default_mode_page(
    UiState& ui,
    Mode mode)
{
    switch (mode)
    {
    case Mode::Settings:
        ui.settings.view = SettingsView::Menu;
        ui.settings.edit_field = 0;
        break;
    case Mode::Calibrate:
        ui.calibrate.state =
            CalibrateState::SelectPump;
        ui.calibrate.measured_volume_ml = 50;
        ui.calibrate.state_started_us = 0;
        break;
    case Mode::Prime:
        ui.prime.state = PrimeState::Setup;
        ui.prime.edit_pump = 0;
        copy_default_volumes(
            ui.prime.target_volume_ml,
            ui.settings);
        clear_pump_values(
            ui.prime.current_volume_ml);
        ui.prime.last_update_us = 0;
        ui.prime.serial_tick_count = 0;
        break;
    case Mode::Sample:
        ui.sample.state = SampleState::Setup;
        ui.sample.start_type =
            SampleStartType::Immediate;
        ui.sample.edit_pump = 0;
        copy_default_volumes(
            ui.sample.target_volume_ml,
            ui.settings);
        clear_pump_values(
            ui.sample.current_volume_ml);
        ui.sample.state_started_us = 0;
        ui.sample.last_update_us = 0;
        ui.sample.serial_tick_count = 0;
        break;
    case Mode::Flush:
        ui.flush.state = FlushState::Setup;
        ui.flush.remaining_seconds =
            ui.flush.duration_minutes * 60;
        ui.flush.last_update_us = 0;
        break;
    case Mode::Log:
        ui.log.view = LogView::List;
        break;
    case Mode::Clock:
        break;
    }
}

bool change_mode(
    UiState& ui,
    Mode new_mode)
{
    if (ui.current_mode_valid &&
        ui.current_mode == new_mode)
    {
        return false;
    }

    leave_current_mode(ui);

    ui.current_mode = new_mode;
    ui.current_mode_valid = true;
    enter_default_mode_page(ui, new_mode);

    std::printf(
        "Mode: %s\n",
        mode_name(new_mode));

    return true;
}

bool increase_value(
    unsigned int& value,
    unsigned int step,
    unsigned int maximum)
{
    if (value >= maximum)
    {
        return false;
    }

    if (maximum - value < step)
    {
        value = maximum;
    }
    else
    {
        value += step;
    }

    return true;
}

bool decrease_value(
    unsigned int& value,
    unsigned int step,
    unsigned int minimum)
{
    if (value <= minimum)
    {
        return false;
    }

    if (value - minimum < step)
    {
        value = minimum;
    }
    else
    {
        value -= step;
    }

    return true;
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

SettingsView settings_view_for_item(
    SettingsItem item)
{
    switch (item)
    {
    case SettingsItem::AdjustClock:
        return SettingsView::AdjustClock;
    case SettingsItem::SamplingSpeed:
        return SettingsView::SamplingSpeed;
    case SettingsItem::DefaultVolumes:
        return SettingsView::DefaultVolumes;
    case SettingsItem::DisplaySettings:
        return SettingsView::DisplaySettings;
    }

    return SettingsView::Menu;
}

bool update_settings(
    UiState& ui,
    const InputEvents& events)
{
    SettingsData& settings = ui.settings;

    if (settings.view == SettingsView::Menu)
    {
        if (events.enter_pressed)
        {
            settings.view =
                settings_view_for_item(
                    settings.selected_item);
            settings.edit_field = 0;

            std::printf(
                "Settings: %s\n",
                settings_item_name(
                    settings.selected_item));
            return true;
        }

        if (events.up_pressed ==
            events.down_pressed)
        {
            return false;
        }

        settings.selected_item =
            events.up_pressed ?
                previous_settings_item(
                    settings.selected_item) :
                next_settings_item(
                    settings.selected_item);

        std::printf(
            "Settings selection: %s\n",
            settings_item_name(
                settings.selected_item));
        return true;
    }

    if (events.enter_pressed)
    {
        if (settings.view ==
            SettingsView::DisplaySettings)
        {
            settings.view = SettingsView::Menu;
        }
        else if (settings.edit_field + 1 <
                 (settings.view ==
                      SettingsView::AdjustClock ?
                      2u :
                      static_cast<unsigned int>(
                          pump_count)))
        {
            ++settings.edit_field;
        }
        else
        {
            settings.view = SettingsView::Menu;
            settings.edit_field = 0;
        }

        return true;
    }

    if (events.up_pressed ==
        events.down_pressed)
    {
        return false;
    }

    bool changed = false;

    if (settings.view ==
        SettingsView::AdjustClock)
    {
        unsigned int& value =
            settings.edit_field == 0 ?
                settings.demo_hour :
                settings.demo_minute;

        const unsigned int maximum =
            settings.edit_field == 0 ? 23 : 59;

        changed = events.up_pressed ?
            increase_value(value, 1, maximum) :
            decrease_value(value, 1, 0);

        if (changed)
        {
            std::printf(
                "Demo clock: %02u:%02u\n",
                settings.demo_hour,
                settings.demo_minute);
        }
    }
    else if (settings.view ==
             SettingsView::SamplingSpeed)
    {
        unsigned int& value =
            settings.pump_speed[
                settings.edit_field];

        changed = events.up_pressed ?
            increase_value(value, 5, 100) :
            decrease_value(value, 5, 0);

        if (changed)
        {
            std::printf(
                "Pump %u speed: %u%%\n",
                static_cast<unsigned int>(
                    settings.edit_field + 1),
                value);
        }
    }
    else if (settings.view ==
             SettingsView::DefaultVolumes)
    {
        unsigned int& value =
            settings.default_volume_ml[
                settings.edit_field];

        changed = events.up_pressed ?
            increase_value(value, 50, 2000) :
            decrease_value(value, 50, 50);

        if (changed)
        {
            std::printf(
                "Pump %u default volume: %u mL\n",
                static_cast<unsigned int>(
                    settings.edit_field + 1),
                value);
        }
    }
    else
    {
        changed = events.up_pressed ?
            increase_value(
                settings.display_level,
                1,
                15) :
            decrease_value(
                settings.display_level,
                1,
                0);

        if (changed)
        {
            std::printf(
                "Display level: %u\n",
                settings.display_level);
        }
    }

    return changed;
}

bool update_calibrate(
    UiState& ui,
    const InputEvents& events,
    std::uint64_t now_us)
{
    CalibrateData& data = ui.calibrate;

    if (data.state ==
        CalibrateState::SelectPump)
    {
        if (events.enter_pressed)
        {
            data.state = CalibrateState::Ready;
            data.measured_volume_ml = 50;

            std::printf(
                "Calibration pump: %u\n",
                static_cast<unsigned int>(
                    data.selected_pump + 1));
            return true;
        }

        if (events.up_pressed ==
            events.down_pressed)
        {
            return false;
        }

        if (events.up_pressed)
        {
            data.selected_pump =
                data.selected_pump == 0 ?
                    pump_count - 1 :
                    data.selected_pump - 1;
        }
        else
        {
            data.selected_pump =
                (data.selected_pump + 1) %
                pump_count;
        }

        std::printf(
            "Calibration pump: %u\n",
            static_cast<unsigned int>(
                data.selected_pump + 1));
        return true;
    }

    if (data.state == CalibrateState::Ready &&
        events.enter_pressed)
    {
        data.state = CalibrateState::Running;
        data.state_started_us = now_us;
        std::printf("Calibration state: RUNNING\n");
        return true;
    }

    if (data.state == CalibrateState::Running &&
        events.enter_pressed)
    {
        data.state = CalibrateState::EnterVolume;
        std::printf(
            "Calibration state: ENTER VOLUME\n");
        return true;
    }

    if (data.state ==
        CalibrateState::EnterVolume)
    {
        if (events.enter_pressed)
        {
            // TODO:
            // Replace with final calibration equation/specification.
            data.coefficient_milli =
                (data.measured_volume_ml * 1000u +
                 data.mock_rotations / 2u) /
                data.mock_rotations;

            data.state = CalibrateState::Result;

            // Prototype mock UART only. No UART peripheral is
            // initialised and no data is sent to an AVR.
            std::printf(
                "Mock UART: Pump %u calibration "
                "coefficient = %u.%03u\n",
                static_cast<unsigned int>(
                    data.selected_pump + 1),
                data.coefficient_milli / 1000u,
                data.coefficient_milli % 1000u);
            return true;
        }

        if (events.up_pressed ==
            events.down_pressed)
        {
            return false;
        }

        const bool changed = events.up_pressed ?
            increase_value(
                data.measured_volume_ml,
                1,
                500) :
            decrease_value(
                data.measured_volume_ml,
                1,
                1);

        if (changed)
        {
            std::printf(
                "Measured volume: %u mL\n",
                data.measured_volume_ml);
        }

        return changed;
    }

    if (data.state == CalibrateState::Result &&
        events.enter_pressed)
    {
        data.state = CalibrateState::SelectPump;
        return true;
    }

    return false;
}

bool update_prime(
    UiState& ui,
    const InputEvents& events,
    std::uint64_t now_us)
{
    PrimeData& data = ui.prime;

    if (data.state == PrimeState::Setup)
    {
        if (events.enter_pressed)
        {
            if (data.edit_pump + 1 < pump_count)
            {
                ++data.edit_pump;
            }
            else
            {
                data.edit_pump = 0;
                clear_pump_values(
                    data.current_volume_ml);
                data.state = PrimeState::Running;
                data.last_update_us = now_us;
                data.serial_tick_count = 0;
                std::printf("Prime state: RUNNING\n");
            }

            return true;
        }

        if (events.up_pressed ==
            events.down_pressed)
        {
            return false;
        }

        unsigned int& value =
            data.target_volume_ml[
                data.edit_pump];

        return events.up_pressed ?
            increase_value(value, 50, 2000) :
            decrease_value(value, 50, 50);
    }

    if (data.state == PrimeState::Running &&
        events.enter_pressed)
    {
        data.state = PrimeState::Stopped;
        std::printf("Prime state: STOPPED\n");
        return true;
    }

    if ((data.state == PrimeState::Complete ||
         data.state == PrimeState::Stopped) &&
        events.enter_pressed)
    {
        data.state = PrimeState::Setup;
        data.edit_pump = 0;
        clear_pump_values(
            data.current_volume_ml);
        return true;
    }

    return false;
}

SampleStartType previous_start_type(
    SampleStartType type)
{
    switch (type)
    {
    case SampleStartType::Immediate:
        return SampleStartType::Delay1Hour;
    case SampleStartType::Delay30Minutes:
        return SampleStartType::Immediate;
    case SampleStartType::Delay1Hour:
        return SampleStartType::Delay30Minutes;
    }

    return SampleStartType::Immediate;
}

SampleStartType next_start_type(
    SampleStartType type)
{
    switch (type)
    {
    case SampleStartType::Immediate:
        return SampleStartType::Delay30Minutes;
    case SampleStartType::Delay30Minutes:
        return SampleStartType::Delay1Hour;
    case SampleStartType::Delay1Hour:
        return SampleStartType::Immediate;
    }

    return SampleStartType::Immediate;
}

bool update_sample(
    UiState& ui,
    const InputEvents& events,
    std::uint64_t now_us)
{
    SampleData& data = ui.sample;

    if (data.state == SampleState::Setup)
    {
        if (events.enter_pressed)
        {
            if (data.edit_pump + 1 < pump_count)
            {
                ++data.edit_pump;
            }
            else
            {
                data.edit_pump = 0;
                data.state = SampleState::StartType;
            }

            return true;
        }

        if (events.up_pressed ==
            events.down_pressed)
        {
            return false;
        }

        unsigned int& value =
            data.target_volume_ml[
                data.edit_pump];

        return events.up_pressed ?
            increase_value(value, 50, 2000) :
            decrease_value(value, 50, 50);
    }

    if (data.state == SampleState::StartType)
    {
        if (events.enter_pressed)
        {
            if (data.start_type ==
                SampleStartType::Immediate)
            {
                data.state = SampleState::Ready;
                std::printf("Sample state: READY\n");
            }
            else
            {
                data.state = SampleState::DelayWaiting;
                data.state_started_us = now_us;
                std::printf(
                    "Sample state: DELAY WAITING\n");
            }

            return true;
        }

        if (events.up_pressed ==
            events.down_pressed)
        {
            return false;
        }

        data.start_type = events.up_pressed ?
            previous_start_type(data.start_type) :
            next_start_type(data.start_type);
        return true;
    }

    if (data.state == SampleState::Ready &&
        events.enter_pressed)
    {
        clear_pump_values(
            data.current_volume_ml);
        data.state = SampleState::Running;
        data.last_update_us = now_us;
        data.serial_tick_count = 0;
        std::printf("Sample state: RUNNING\n");
        return true;
    }

    if (data.state == SampleState::Running &&
        events.enter_pressed)
    {
        data.state = SampleState::Stopped;
        std::printf("Sample state: STOPPED\n");
        return true;
    }

    if ((data.state == SampleState::Complete ||
         data.state == SampleState::Stopped) &&
        events.enter_pressed)
    {
        data.state = SampleState::Setup;
        data.edit_pump = 0;
        clear_pump_values(
            data.current_volume_ml);
        return true;
    }

    return false;
}

bool update_flush(
    UiState& ui,
    const InputEvents& events,
    std::uint64_t now_us)
{
    FlushData& data = ui.flush;

    if (data.state == FlushState::Setup)
    {
        if (events.enter_pressed)
        {
            data.remaining_seconds =
                data.duration_minutes * 60;
            data.state = FlushState::Running;
            data.last_update_us = now_us;
            std::printf("Flush state: RUNNING\n");
            return true;
        }

        if (events.up_pressed ==
            events.down_pressed)
        {
            return false;
        }

        return events.up_pressed ?
            increase_value(
                data.duration_minutes,
                1,
                10) :
            decrease_value(
                data.duration_minutes,
                1,
                1);
    }

    if (data.state == FlushState::Running &&
        events.enter_pressed)
    {
        data.state = FlushState::Stopped;
        std::printf("Flush state: STOPPED\n");
        return true;
    }

    if ((data.state == FlushState::Complete ||
         data.state == FlushState::Stopped) &&
        events.enter_pressed)
    {
        data.state = FlushState::Setup;
        return true;
    }

    return false;
}

bool update_log(
    UiState& ui,
    const InputEvents& events)
{
    LogData& data = ui.log;

    if (data.view == LogView::Detail)
    {
        if (events.enter_pressed)
        {
            data.view = LogView::List;
            return true;
        }

        return false;
    }

    if (events.enter_pressed)
    {
        data.view = LogView::Detail;
        return true;
    }

    if (events.up_pressed ==
        events.down_pressed)
    {
        return false;
    }

    if (events.up_pressed)
    {
        data.selected_log =
            data.selected_log == 0 ?
                log_count - 1 :
                data.selected_log - 1;
    }
    else
    {
        data.selected_log =
            (data.selected_log + 1) %
            log_count;
    }

    return true;
}

bool update_current_mode(
    UiState& ui,
    const InputEvents& events,
    std::uint64_t now_us)
{
    if (!ui.current_mode_valid)
    {
        return false;
    }

    switch (ui.current_mode)
    {
    case Mode::Settings:
        return update_settings(ui, events);
    case Mode::Calibrate:
        return update_calibrate(
            ui,
            events,
            now_us);
    case Mode::Prime:
        return update_prime(
            ui,
            events,
            now_us);
    case Mode::Sample:
        return update_sample(
            ui,
            events,
            now_us);
    case Mode::Flush:
        return update_flush(
            ui,
            events,
            now_us);
    case Mode::Log:
        return update_log(ui, events);
    case Mode::Clock:
        return false;
    }

    return false;
}

bool all_pumps_reached_target(
    const unsigned int current[pump_count],
    const unsigned int target[pump_count])
{
    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        if (current[pump] < target[pump])
        {
            return false;
        }
    }

    return true;
}

bool advance_mock_pump_volumes(
    unsigned int current[pump_count],
    const unsigned int target[pump_count],
    const unsigned int increments[pump_count],
    std::uint64_t tick_count)
{
    bool changed = false;

    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        if (current[pump] >= target[pump])
        {
            continue;
        }

        const std::uint64_t advance =
            static_cast<std::uint64_t>(
                increments[pump]) *
            tick_count;

        const std::uint64_t new_volume =
            static_cast<std::uint64_t>(
                current[pump]) +
            advance;

        current[pump] =
            new_volume >= target[pump] ?
                target[pump] :
                static_cast<unsigned int>(
                    new_volume);

        changed = true;
    }

    return changed;
}

void print_mock_avr_volume(
    const unsigned int volume[pump_count])
{
    // Future AVR UART integration point: replace these prototype
    // RAM values with parsed volume messages from the AVR.
    std::printf(
        "Mock AVR volume: "
        "P1=%u P2=%u P3=%u P4=%u\n",
        volume[0],
        volume[1],
        volume[2],
        volume[3]);
}

bool update_calibration_mock(
    UiState& ui,
    std::uint64_t now_us)
{
    CalibrateData& data = ui.calibrate;

    if (data.state != CalibrateState::Running ||
        now_us - data.state_started_us <
            calibration_run_time_us)
    {
        return false;
    }

    data.state = CalibrateState::EnterVolume;
    std::printf(
        "Calibration state: ENTER VOLUME\n");
    return true;
}

bool update_prime_mock(
    UiState& ui,
    std::uint64_t now_us)
{
    PrimeData& data = ui.prime;

    if (data.state != PrimeState::Running)
    {
        return false;
    }

    const std::uint64_t tick_count =
        (now_us - data.last_update_us) /
        pump_update_interval_us;

    if (tick_count == 0)
    {
        return false;
    }

    data.last_update_us +=
        tick_count * pump_update_interval_us;

    constexpr unsigned int increments[pump_count] = {
        5, 4, 6, 5
    };

    const bool changed =
        advance_mock_pump_volumes(
            data.current_volume_ml,
            data.target_volume_ml,
            increments,
            tick_count);

    data.serial_tick_count += tick_count;

    if (data.serial_tick_count >= 4)
    {
        print_mock_avr_volume(
            data.current_volume_ml);
        data.serial_tick_count %= 4;
    }

    if (all_pumps_reached_target(
            data.current_volume_ml,
            data.target_volume_ml))
    {
        data.state = PrimeState::Complete;
        std::printf("Prime state: COMPLETE\n");
        return true;
    }

    return changed;
}

bool update_sample_mock(
    UiState& ui,
    std::uint64_t now_us)
{
    SampleData& data = ui.sample;

    if (data.state == SampleState::DelayWaiting)
    {
        // Prototype only:
        // A 3-second simulation represents the selected real delay.
        if (now_us - data.state_started_us >=
            sample_delay_demo_time_us)
        {
            data.state = SampleState::Ready;
            std::printf("Sample state: READY\n");
            return true;
        }

        return false;
    }

    if (data.state != SampleState::Running)
    {
        return false;
    }

    const std::uint64_t tick_count =
        (now_us - data.last_update_us) /
        pump_update_interval_us;

    if (tick_count == 0)
    {
        return false;
    }

    data.last_update_us +=
        tick_count * pump_update_interval_us;

    constexpr unsigned int increments[pump_count] = {
        20, 18, 22, 19
    };

    const bool changed =
        advance_mock_pump_volumes(
            data.current_volume_ml,
            data.target_volume_ml,
            increments,
            tick_count);

    data.serial_tick_count += tick_count;

    if (data.serial_tick_count >= 4)
    {
        print_mock_avr_volume(
            data.current_volume_ml);
        data.serial_tick_count %= 4;
    }

    if (all_pumps_reached_target(
            data.current_volume_ml,
            data.target_volume_ml))
    {
        data.state = SampleState::Complete;
        std::printf("Sample state: COMPLETE\n");
        return true;
    }

    return changed;
}

bool update_flush_mock(
    UiState& ui,
    std::uint64_t now_us)
{
    FlushData& data = ui.flush;

    if (data.state != FlushState::Running)
    {
        return false;
    }

    const std::uint64_t tick_count =
        (now_us - data.last_update_us) /
        flush_update_interval_us;

    if (tick_count == 0)
    {
        return false;
    }

    data.last_update_us +=
        tick_count * flush_update_interval_us;

    // Prototype accelerated timer: every 0.5 real seconds represents
    // 6 simulated seconds, so 02:00 completes in about 10 seconds.
    const std::uint64_t simulated_seconds =
        tick_count * 6u;

    if (simulated_seconds >=
        data.remaining_seconds)
    {
        data.remaining_seconds = 0;
        data.state = FlushState::Complete;
        std::printf("Flush state: COMPLETE\n");
    }
    else
    {
        data.remaining_seconds -=
            static_cast<unsigned int>(
                simulated_seconds);
    }

    return true;
}

bool update_mock_data(
    UiState& ui,
    std::uint64_t now_us)
{
    if (!ui.current_mode_valid)
    {
        return false;
    }

    switch (ui.current_mode)
    {
    case Mode::Calibrate:
        return update_calibration_mock(
            ui,
            now_us);
    case Mode::Prime:
        return update_prime_mock(ui, now_us);
    case Mode::Sample:
        return update_sample_mock(ui, now_us);
    case Mode::Flush:
        return update_flush_mock(ui, now_us);
    case Mode::Settings:
    case Mode::Log:
    case Mode::Clock:
        return false;
    }

    return false;
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

bool format_succeeded(
    int written,
    std::size_t buffer_size)
{
    return written > 0 &&
           static_cast<std::size_t>(written) <
               buffer_size;
}

bool draw_centered_text(
    std::int32_t y,
    const char* text,
    std::uint8_t foreground = bright,
    std::uint8_t background = black)
{
    const std::size_t length =
        text_length(text);

    if (length == 0 ||
        length >
            framebuffer::width /
                static_cast<std::size_t>(
                    glyph_width))
    {
        return false;
    }

    const std::size_t pixel_width =
        length *
        static_cast<std::size_t>(
            glyph_width);

    const std::int32_t x =
        static_cast<std::int32_t>(
            (framebuffer::width - pixel_width) /
            2);

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

bool draw_dot_cell(
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

    return graphics::fill_rectangle(
        x + 3,
        y + 6,
        2,
        2,
        foreground);
}

bool draw_time(
    std::int32_t x,
    std::int32_t y,
    unsigned int hour,
    unsigned int minute,
    std::uint8_t foreground = bright,
    std::uint8_t background = black)
{
    char time_text[6];

    const int written = std::snprintf(
        time_text,
        sizeof(time_text),
        "%02u %02u",
        hour,
        minute);

    if (!format_succeeded(
            written,
            sizeof(time_text)))
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

bool draw_duration(
    std::int32_t x,
    std::int32_t y,
    unsigned int total_seconds,
    std::uint8_t foreground = bright,
    std::uint8_t background = black)
{
    return draw_time(
        x,
        y,
        total_seconds / 60u,
        total_seconds % 60u,
        foreground,
        background);
}

bool draw_percentage_right(
    unsigned int percentage,
    std::int32_t right_edge,
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

    if (!format_succeeded(
            written,
            sizeof(digits)))
    {
        return false;
    }

    const std::int32_t digit_width =
        written * glyph_width;
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

bool draw_fraction_right(
    unsigned int current,
    unsigned int target,
    std::int32_t right_edge,
    std::int32_t y,
    std::uint8_t foreground = bright,
    std::uint8_t background = black)
{
    char current_text[5];
    char target_text[5];

    const int current_written = std::snprintf(
        current_text,
        sizeof(current_text),
        "%u",
        current);

    const int target_written = std::snprintf(
        target_text,
        sizeof(target_text),
        "%u",
        target);

    if (!format_succeeded(
            current_written,
            sizeof(current_text)) ||
        !format_succeeded(
            target_written,
            sizeof(target_text)))
    {
        return false;
    }

    char fraction_text[10];

    const int written = std::snprintf(
        fraction_text,
        sizeof(fraction_text),
        "%s %s",
        current_text,
        target_text);

    if (!format_succeeded(
            written,
            sizeof(fraction_text)))
    {
        return false;
    }

    const std::int32_t x =
        right_edge - written * glyph_width;

    if (!graphics::draw_text(
            x,
            y,
            fraction_text,
            foreground,
            background))
    {
        return false;
    }

    return draw_slash_cell(
        x + current_written * glyph_width,
        y,
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

    for (unsigned int segment = 0;
         segment < 4;
         ++segment)
    {
        const std::uint8_t level =
            segment < demo_battery_level ?
                bright : black;

        if (!graphics::fill_rectangle(
                6 +
                    static_cast<std::int32_t>(
                        segment) * 6,
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
                17, 3, 14, 6, black) ||
            !graphics::draw_line(
                14, 6, 18, 6, black) ||
            !graphics::draw_line(
                18, 6, 15, 9, black))
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

    if (!draw_centered_text(
            2,
            mode_header_text(
                ui.current_mode)))
    {
        return false;
    }

    if (!draw_time(
            216,
            2,
            ui.settings.demo_hour,
            ui.settings.demo_minute))
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

bool draw_selection_row(
    std::int32_t text_y,
    const char* text,
    bool selected)
{
    const std::uint8_t background =
        selected ? highlight : black;
    const std::uint8_t foreground =
        selected ? black : bright;

    if (!graphics::fill_rectangle(
            4,
            text_y - 1,
            248,
            10,
            background))
    {
        return false;
    }

    return draw_centered_text(
        text_y,
        text,
        foreground,
        background);
}

bool render_settings_menu(const UiState& ui)
{
    constexpr SettingsItem items[] = {
        SettingsItem::AdjustClock,
        SettingsItem::SamplingSpeed,
        SettingsItem::DefaultVolumes,
        SettingsItem::DisplaySettings
    };

    constexpr std::int32_t rows[] = {
        15, 27, 39, 51
    };

    for (std::size_t index = 0;
         index < pump_count;
         ++index)
    {
        if (!draw_selection_row(
                rows[index],
                settings_item_name(items[index]),
                ui.settings.selected_item ==
                    items[index]))
        {
            return false;
        }
    }

    return true;
}

bool render_clock_field_row(
    const char* label,
    unsigned int value,
    std::int32_t text_y,
    bool selected)
{
    const std::uint8_t background =
        selected ? highlight : black;
    const std::uint8_t foreground =
        selected ? black : bright;

    if (!graphics::fill_rectangle(
            40,
            text_y - 1,
            176,
            10,
            background))
    {
        return false;
    }

    if (!graphics::draw_text(
            48,
            text_y,
            label,
            foreground,
            background))
    {
        return false;
    }

    char value_text[3];

    const int written = std::snprintf(
        value_text,
        sizeof(value_text),
        "%02u",
        value);

    if (!format_succeeded(
            written,
            sizeof(value_text)))
    {
        return false;
    }

    return graphics::draw_text(
        184,
        text_y,
        value_text,
        foreground,
        background);
}

bool render_adjust_clock(const UiState& ui)
{
    if (!draw_centered_text(
            15,
            "ADJUST CLOCK"))
    {
        return false;
    }

    if (!render_clock_field_row(
            "HOUR",
            ui.settings.demo_hour,
            30,
            ui.settings.edit_field == 0))
    {
        return false;
    }

    return render_clock_field_row(
        "MINUTE",
        ui.settings.demo_minute,
        46,
        ui.settings.edit_field == 1);
}

bool render_pump_speed_row(
    std::size_t pump,
    unsigned int speed,
    std::int32_t text_y,
    bool selected)
{
    const std::uint8_t background =
        selected ? highlight : black;
    const std::uint8_t foreground =
        selected ? black : bright;

    if (!graphics::fill_rectangle(
            56,
            text_y - 1,
            144,
            10,
            background))
    {
        return false;
    }

    char pump_text[3];

    const int written = std::snprintf(
        pump_text,
        sizeof(pump_text),
        "P%u",
        static_cast<unsigned int>(pump + 1));

    if (!format_succeeded(
            written,
            sizeof(pump_text)) ||
        !graphics::draw_text(
            72,
            text_y,
            pump_text,
            foreground,
            background))
    {
        return false;
    }

    return draw_percentage_right(
        speed,
        184,
        text_y,
        foreground,
        background);
}

bool render_sampling_speed(const UiState& ui)
{
    if (!draw_centered_text(
            14,
            "SAMPLING SPEED"))
    {
        return false;
    }

    constexpr std::int32_t rows[pump_count] = {
        24, 34, 44, 54
    };

    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        if (!render_pump_speed_row(
                pump,
                ui.settings.pump_speed[pump],
                rows[pump],
                ui.settings.edit_field == pump))
        {
            return false;
        }
    }

    return true;
}

bool render_pump_volume_row(
    std::size_t pump,
    unsigned int volume_ml,
    std::int32_t text_y,
    bool selected,
    std::int32_t left = 40,
    std::int32_t width = 176)
{
    const std::uint8_t background =
        selected ? highlight : black;
    const std::uint8_t foreground =
        selected ? black : bright;

    if (!graphics::fill_rectangle(
            left,
            text_y - 1,
            width,
            10,
            background))
    {
        return false;
    }

    char value_text[13];

    const int written = std::snprintf(
        value_text,
        sizeof(value_text),
        "P%u %u mL",
        static_cast<unsigned int>(pump + 1),
        volume_ml);

    if (!format_succeeded(
            written,
            sizeof(value_text)))
    {
        return false;
    }

    return draw_centered_text(
        text_y,
        value_text,
        foreground,
        background);
}

bool render_default_volumes(const UiState& ui)
{
    if (!draw_centered_text(
            14,
            "DEFAULT VOLUMES"))
    {
        return false;
    }

    constexpr std::int32_t rows[pump_count] = {
        24, 34, 44, 54
    };

    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        if (!render_pump_volume_row(
                pump,
                ui.settings.default_volume_ml[pump],
                rows[pump],
                ui.settings.edit_field == pump))
        {
            return false;
        }
    }

    return true;
}

bool render_display_settings(const UiState& ui)
{
    if (!draw_centered_text(17, "DISPLAY") ||
        !draw_centered_text(31, "CONTRAST"))
    {
        return false;
    }

    if (!graphics::fill_rectangle(
            72,
            45,
            112,
            10,
            highlight))
    {
        return false;
    }

    char level_text[9];

    const int written = std::snprintf(
        level_text,
        sizeof(level_text),
        "LEVEL %u",
        ui.settings.display_level);

    if (!format_succeeded(
            written,
            sizeof(level_text)))
    {
        return false;
    }

    return draw_centered_text(
        47,
        level_text,
        black,
        highlight);
}

bool render_settings(const UiState& ui)
{
    switch (ui.settings.view)
    {
    case SettingsView::Menu:
        return render_settings_menu(ui);
    case SettingsView::AdjustClock:
        return render_adjust_clock(ui);
    case SettingsView::SamplingSpeed:
        return render_sampling_speed(ui);
    case SettingsView::DefaultVolumes:
        return render_default_volumes(ui);
    case SettingsView::DisplaySettings:
        return render_display_settings(ui);
    }

    return false;
}

bool render_calibration_pump_text(
    std::size_t pump,
    std::int32_t y)
{
    char pump_text[7];

    const int written = std::snprintf(
        pump_text,
        sizeof(pump_text),
        "PUMP %u",
        static_cast<unsigned int>(pump + 1));

    return format_succeeded(
               written,
               sizeof(pump_text)) &&
           draw_centered_text(y, pump_text);
}

bool render_calibrate_select(const UiState& ui)
{
    if (!draw_centered_text(14, "CALIBRATE"))
    {
        return false;
    }

    constexpr std::int32_t rows[pump_count] = {
        24, 34, 44, 54
    };

    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        char pump_text[7];

        const int written = std::snprintf(
            pump_text,
            sizeof(pump_text),
            "PUMP %u",
            static_cast<unsigned int>(pump + 1));

        if (!format_succeeded(
                written,
                sizeof(pump_text)) ||
            !draw_selection_row(
                rows[pump],
                pump_text,
                ui.calibrate.selected_pump == pump))
        {
            return false;
        }
    }

    return true;
}

bool render_calibrate(const UiState& ui)
{
    const CalibrateData& data = ui.calibrate;

    if (data.state ==
        CalibrateState::SelectPump)
    {
        return render_calibrate_select(ui);
    }

    if (data.state == CalibrateState::Ready)
    {
        return draw_centered_text(16, "CALIBRATE") &&
               render_calibration_pump_text(
                   data.selected_pump,
                   32) &&
               draw_centered_text(48, "READY");
    }

    if (data.state == CalibrateState::Running)
    {
        char rotations_text[4];

        const int written = std::snprintf(
            rotations_text,
            sizeof(rotations_text),
            "%u",
            data.mock_rotations);

        return format_succeeded(
                   written,
                   sizeof(rotations_text)) &&
               draw_centered_text(14, "CALIBRATE") &&
               render_calibration_pump_text(
                   data.selected_pump,
                   24) &&
               draw_centered_text(34, "ROTATIONS") &&
               draw_centered_text(44, rotations_text) &&
               draw_centered_text(54, "RUNNING");
    }

    if (data.state ==
        CalibrateState::EnterVolume)
    {
        char volume_text[11];

        const int written = std::snprintf(
            volume_text,
            sizeof(volume_text),
            "%u mL",
            data.measured_volume_ml);

        return format_succeeded(
                   written,
                   sizeof(volume_text)) &&
               draw_centered_text(17, "DISPENSED") &&
               render_calibration_pump_text(
                   data.selected_pump,
                   33) &&
               draw_centered_text(49, volume_text);
    }

    char pump_text[3];
    char rotations_text[8];
    char volume_text[11];
    char coefficient_text[8];

    const int pump_written = std::snprintf(
        pump_text,
        sizeof(pump_text),
        "P%u",
        static_cast<unsigned int>(
            data.selected_pump + 1));

    const int rotations_written = std::snprintf(
        rotations_text,
        sizeof(rotations_text),
        "%u ROT",
        data.mock_rotations);

    const int volume_written = std::snprintf(
        volume_text,
        sizeof(volume_text),
        "%u mL",
        data.measured_volume_ml);

    const int coefficient_written = std::snprintf(
        coefficient_text,
        sizeof(coefficient_text),
        "K %u %03u",
        data.coefficient_milli / 1000u,
        data.coefficient_milli % 1000u);

    if (!format_succeeded(
            pump_written,
            sizeof(pump_text)) ||
        !format_succeeded(
            rotations_written,
            sizeof(rotations_text)) ||
        !format_succeeded(
            volume_written,
            sizeof(volume_text)) ||
        !format_succeeded(
            coefficient_written,
            sizeof(coefficient_text)) ||
        !draw_centered_text(14, "CAL RESULT") ||
        !draw_centered_text(24, pump_text) ||
        !draw_centered_text(34, rotations_text) ||
        !draw_centered_text(44, volume_text))
    {
        return false;
    }

    const std::int32_t coefficient_x =
        static_cast<std::int32_t>(
            (framebuffer::width -
             static_cast<std::size_t>(
                 coefficient_written *
                 glyph_width)) /
            2);

    if (!graphics::draw_text(
            coefficient_x,
            54,
            coefficient_text,
            bright,
            black))
    {
        return false;
    }

    return draw_dot_cell(
        coefficient_x + 3 * glyph_width,
        54,
        bright,
        black);
}

bool render_pump_setup(
    const char* title,
    const unsigned int target[pump_count],
    std::size_t selected_pump)
{
    if (!draw_centered_text(14, title))
    {
        return false;
    }

    constexpr std::int32_t rows[pump_count] = {
        24, 34, 44, 54
    };

    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        if (!render_pump_volume_row(
                pump,
                target[pump],
                rows[pump],
                pump == selected_pump,
                4,
                248))
        {
            return false;
        }
    }

    return true;
}

bool render_pump_fraction_screen(
    const char* title,
    const unsigned int current[pump_count],
    const unsigned int target[pump_count])
{
    if (!draw_centered_text(14, title))
    {
        return false;
    }

    constexpr std::int32_t rows[pump_count] = {
        24, 34, 44, 54
    };

    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        char pump_text[3];

        const int written = std::snprintf(
            pump_text,
            sizeof(pump_text),
            "P%u",
            static_cast<unsigned int>(pump + 1));

        if (!format_succeeded(
                written,
                sizeof(pump_text)) ||
            !graphics::draw_text(
                48,
                rows[pump],
                pump_text,
                bright,
                black) ||
            !draw_fraction_right(
                current[pump],
                target[pump],
                208,
                rows[pump]))
        {
            return false;
        }
    }

    return true;
}

bool render_pump_values_screen(
    const char* title,
    const unsigned int current[pump_count])
{
    if (!draw_centered_text(14, title))
    {
        return false;
    }

    constexpr std::int32_t rows[pump_count] = {
        24, 34, 44, 54
    };

    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        char value_text[9];

        const int written = std::snprintf(
            value_text,
            sizeof(value_text),
            "P%u %u",
            static_cast<unsigned int>(pump + 1),
            current[pump]);

        if (!format_succeeded(
                written,
                sizeof(value_text)) ||
            !draw_centered_text(
                rows[pump],
                value_text))
        {
            return false;
        }
    }

    return true;
}

bool render_prime(const UiState& ui)
{
    const PrimeData& data = ui.prime;

    switch (data.state)
    {
    case PrimeState::Setup:
        return render_pump_setup(
            "SETUP",
            data.target_volume_ml,
            data.edit_pump);
    case PrimeState::Running:
        return render_pump_fraction_screen(
            "RUNNING",
            data.current_volume_ml,
            data.target_volume_ml);
    case PrimeState::Complete:
        return render_pump_values_screen(
            "COMPLETE",
            data.current_volume_ml);
    case PrimeState::Stopped:
        return render_pump_values_screen(
            "STOPPED",
            data.current_volume_ml);
    }

    return false;
}

bool render_sample_start_type(const UiState& ui)
{
    if (!draw_centered_text(14, "SAMPLE START"))
    {
        return false;
    }

    if (!draw_selection_row(
            27,
            "IMMEDIATE",
            ui.sample.start_type ==
                SampleStartType::Immediate))
    {
        return false;
    }

    if (!draw_selection_row(
            39,
            "DELAY 30 MIN",
            ui.sample.start_type ==
                SampleStartType::Delay30Minutes))
    {
        return false;
    }

    return draw_selection_row(
        51,
        "DELAY 1 HOUR",
        ui.sample.start_type ==
            SampleStartType::Delay1Hour);
}

bool render_sample_delay(const UiState& ui)
{
    const char* delay_text =
        ui.sample.start_type ==
                SampleStartType::Delay30Minutes ?
            "30 MIN" :
            "1 HOUR";

    return draw_centered_text(17, "DELAYED") &&
           draw_centered_text(33, delay_text) &&
           draw_centered_text(49, "WAITING");
}

bool render_sample_ready()
{
    return draw_centered_text(22, "READY") &&
           draw_centered_text(42, "P1 P2 P3 P4");
}

bool render_sample_running(const UiState& ui)
{
    if (!draw_centered_text(14, "RUNNING"))
    {
        return false;
    }

    constexpr std::int32_t rows[pump_count] = {
        24, 34, 44, 54
    };

    for (std::size_t pump = 0;
         pump < pump_count;
         ++pump)
    {
        char pump_text[3];

        const int written = std::snprintf(
            pump_text,
            sizeof(pump_text),
            "P%u",
            static_cast<unsigned int>(pump + 1));

        if (!format_succeeded(
                written,
                sizeof(pump_text)) ||
            !graphics::draw_text(
                4,
                rows[pump],
                pump_text,
                bright,
                black) ||
            !graphics::draw_rectangle(
                24,
                rows[pump] + 1,
                144,
                6,
                dim))
        {
            return false;
        }

        const unsigned int target =
            ui.sample.target_volume_ml[pump];
        const unsigned int current =
            ui.sample.current_volume_ml[pump];

        const unsigned int limited_current =
            current > target ? target : current;

        const std::int32_t fill_width =
            target == 0 ?
                0 :
                static_cast<std::int32_t>(
                    (static_cast<std::uint64_t>(
                         limited_current) *
                     142u) /
                    target);

        if (fill_width > 0 &&
            !graphics::fill_rectangle(
                25,
                rows[pump] + 2,
                fill_width,
                4,
                highlight))
        {
            return false;
        }

        if (!draw_fraction_right(
                current,
                target,
                252,
                rows[pump]))
        {
            return false;
        }
    }

    return true;
}

bool render_sample(const UiState& ui)
{
    const SampleData& data = ui.sample;

    switch (data.state)
    {
    case SampleState::Setup:
        return render_pump_setup(
            "SETUP",
            data.target_volume_ml,
            data.edit_pump);
    case SampleState::StartType:
        return render_sample_start_type(ui);
    case SampleState::DelayWaiting:
        return render_sample_delay(ui);
    case SampleState::Ready:
        return render_sample_ready();
    case SampleState::Running:
        return render_sample_running(ui);
    case SampleState::Complete:
        return render_pump_values_screen(
            "COMPLETE",
            data.current_volume_ml);
    case SampleState::Stopped:
        return render_pump_values_screen(
            "STOPPED",
            data.current_volume_ml);
    }

    return false;
}

bool render_flush(const UiState& ui)
{
    const FlushData& data = ui.flush;

    if (data.state == FlushState::Setup)
    {
        return draw_centered_text(20, "DURATION") &&
               draw_duration(
                   108,
                   40,
                   data.duration_minutes * 60);
    }

    if (data.state == FlushState::Running)
    {
        return draw_centered_text(20, "RUNNING") &&
               draw_duration(
                   108,
                   40,
                   data.remaining_seconds);
    }

    return draw_centered_text(
        31,
        data.state == FlushState::Complete ?
            "COMPLETE" :
            "STOPPED");
}

bool draw_log_timestamp(
    const SampleLog& log,
    std::int32_t x,
    std::int32_t y,
    std::uint8_t foreground,
    std::uint8_t background)
{
    char timestamp[15];

    const int written = std::snprintf(
        timestamp,
        sizeof(timestamp),
        "%02u %s %02u %02u",
        log.day,
        log.month,
        log.hour,
        log.minute);

    if (!format_succeeded(
            written,
            sizeof(timestamp)) ||
        !graphics::draw_text(
            x,
            y,
            timestamp,
            foreground,
            background))
    {
        return false;
    }

    return draw_colon_cell(
        x + 9 * glyph_width,
        y,
        foreground,
        background);
}

bool render_log_list(const UiState& ui)
{
    if (!draw_centered_text(
            15,
            "PREVIOUS SAMPLES"))
    {
        return false;
    }

    constexpr std::int32_t rows[log_count] = {
        29, 41, 53
    };

    for (std::size_t index = 0;
         index < log_count;
         ++index)
    {
        const bool selected =
            ui.log.selected_log == index;

        const std::uint8_t background =
            selected ? highlight : black;
        const std::uint8_t foreground =
            selected ? black : bright;

        if (!graphics::fill_rectangle(
                4,
                rows[index] - 1,
                248,
                10,
                background) ||
            !draw_log_timestamp(
                sample_logs[index],
                80,
                rows[index],
                foreground,
                background))
        {
            return false;
        }
    }

    return true;
}

bool render_log_detail(const UiState& ui)
{
    const SampleLog& log =
        sample_logs[ui.log.selected_log];

    if (!draw_log_timestamp(
            log,
            80,
            16,
            bright,
            black))
    {
        return false;
    }

    char first_pumps[16];
    char second_pumps[16];

    const int first_written = std::snprintf(
        first_pumps,
        sizeof(first_pumps),
        "P1 %u P2 %u",
        log.volume_ml[0],
        log.volume_ml[1]);

    const int second_written = std::snprintf(
        second_pumps,
        sizeof(second_pumps),
        "P3 %u P4 %u",
        log.volume_ml[2],
        log.volume_ml[3]);

    if (!format_succeeded(
            first_written,
            sizeof(first_pumps)) ||
        !format_succeeded(
            second_written,
            sizeof(second_pumps)) ||
        !draw_centered_text(29, first_pumps) ||
        !draw_centered_text(41, second_pumps))
    {
        return false;
    }

    char summary[13];

    const int summary_written = std::snprintf(
        summary,
        sizeof(summary),
        "%02u %02u %u %uV",
        log.duration_seconds / 60u,
        log.duration_seconds % 60u,
        log.battery_decivolts / 10u,
        log.battery_decivolts % 10u);

    if (!format_succeeded(
            summary_written,
            sizeof(summary)))
    {
        return false;
    }

    const std::int32_t summary_x =
        static_cast<std::int32_t>(
            (framebuffer::width -
             static_cast<std::size_t>(
                 summary_written *
                 glyph_width)) /
            2);

    if (!graphics::draw_text(
            summary_x,
            53,
            summary,
            bright,
            black) ||
        !draw_colon_cell(
            summary_x + 2 * glyph_width,
            53,
            bright,
            black))
    {
        return false;
    }

    return draw_dot_cell(
        summary_x + 8 * glyph_width,
        53,
        bright,
        black);
}

bool render_log(const UiState& ui)
{
    return ui.log.view == LogView::List ?
        render_log_list(ui) :
        render_log_detail(ui);
}

bool draw_large_digit(
    std::int32_t x,
    std::int32_t y,
    char digit)
{
    const font8x8::Glyph* glyph =
        font8x8::find_glyph(digit);

    if (glyph == nullptr)
    {
        return false;
    }

    bool drew_pixel = false;

    for (std::size_t row = 0;
         row < font8x8::glyph_height;
         ++row)
    {
        for (std::size_t column = 0;
             column < font8x8::glyph_width;
             ++column)
        {
            const std::uint8_t mask =
                static_cast<std::uint8_t>(
                    0x80u >> column);

            if (((*glyph)[row] & mask) == 0)
            {
                continue;
            }

            if (!graphics::fill_rectangle(
                    x + static_cast<std::int32_t>(
                            column * 2),
                    y + static_cast<std::int32_t>(
                            row * 2),
                    2,
                    2,
                    bright))
            {
                return false;
            }

            drew_pixel = true;
        }
    }

    return drew_pixel;
}

bool draw_large_time(
    const SettingsData& settings)
{
    const char digits[] = {
        static_cast<char>(
            '0' + settings.demo_hour / 10u),
        static_cast<char>(
            '0' + settings.demo_hour % 10u),
        static_cast<char>(
            '0' + settings.demo_minute / 10u),
        static_cast<char>(
            '0' + settings.demo_minute % 10u)
    };

    constexpr std::int32_t x = 88;
    constexpr std::int32_t y = 24;

    if (!draw_large_digit(x, y, digits[0]) ||
        !draw_large_digit(x + 16, y, digits[1]) ||
        !graphics::fill_rectangle(
            x + 38, y + 3, 4, 4, bright) ||
        !graphics::fill_rectangle(
            x + 38, y + 10, 4, 4, bright) ||
        !draw_large_digit(x + 48, y, digits[2]) ||
        !draw_large_digit(x + 64, y, digits[3]))
    {
        return false;
    }

    return true;
}

bool render_clock(const UiState& ui)
{
    if (!draw_centered_text(14, "CLOCK") ||
        !draw_large_time(ui.settings))
    {
        return false;
    }

    char date_text[12];

    const int written = std::snprintf(
        date_text,
        sizeof(date_text),
        "%02u %s %u",
        demo_day,
        demo_month,
        demo_year);

    return format_succeeded(
               written,
               sizeof(date_text)) &&
           draw_centered_text(48, date_text);
}

bool render_current_page(const UiState& ui)
{
    if (!render_status_bar(ui))
    {
        return false;
    }

    switch (ui.current_mode)
    {
    case Mode::Settings:
        return render_settings(ui);
    case Mode::Calibrate:
        return render_calibrate(ui);
    case Mode::Prime:
        return render_prime(ui);
    case Mode::Sample:
        return render_sample(ui);
    case Mode::Flush:
        return render_flush(ui);
    case Mode::Log:
        return render_log(ui);
    case Mode::Clock:
        return render_clock(ui);
    }

    return false;
}

void refresh_display(const UiState& ui)
{
    framebuffer::clear(black);

    if (!render_current_page(ui))
    {
        halt_with_message(
            "Medusa Full UI rendering failed");
    }

    if (!ssd1322::write_full_frame(
            framebuffer::data(),
            framebuffer::data_size()))
    {
        halt_with_message(
            "Medusa Full UI frame transfer failed");
    }
}
}

int main()
{
    stdio_init_all();

    ssd1322::initialise_hardware();
    initialise_inputs();

    sleep_ms(1000);

    std::printf(
        "=== MEDUSA FULL UI PROTOTYPE ===\n");
    std::printf("UP pin: GP2\n");
    std::printf("DOWN pin: GP3\n");
    std::printf("ENTER pin: GP4\n");
    std::printf("Rotary position 1: GP5 SETTINGS\n");
    std::printf("Rotary position 2: GP6 CALIBRATE\n");
    std::printf("Rotary position 3: GP7 PRIME\n");
    std::printf("Rotary position 4: GP8 SAMPLE\n");
    std::printf("Rotary position 5: GP9 FLUSH\n");
    std::printf("Rotary position 6: GP10 LOG\n");
    std::printf("Rotary position 7: GP11 CLOCK\n");
    std::printf("Rotary position 8: GP12 RESERVED\n");
    std::printf(
        "All inputs use RP2040 internal pull-ups\n");
    std::printf("Buttons and selections are active-low\n");

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

    ButtonState up_button = make_button_state(
        UP_BUTTON_PIN,
        "UP",
        initial_gpio_snapshot,
        initial_time_us);

    ButtonState down_button = make_button_state(
        DOWN_BUTTON_PIN,
        "DOWN",
        initial_gpio_snapshot,
        initial_time_us);

    ButtonState enter_button = make_button_state(
        ENTER_BUTTON_PIN,
        "ENTER",
        initial_gpio_snapshot,
        initial_time_us);

    RotaryState rotary_state;
    UiState ui;

    while (true)
    {
        const std::uint64_t now_us =
            time_us_64();
        const std::uint32_t gpio_snapshot =
            gpio_get_all();

        bool ui_dirty = false;
        bool mode_changed = false;

        const RotaryEvent rotary_event =
            update_rotary(
                rotary_state,
                read_rotary(gpio_snapshot),
                now_us);

        if (rotary_event.kind ==
            RotaryEventKind::ModeSelected)
        {
            mode_changed = change_mode(
                ui,
                rotary_event.mode);
            ui_dirty = mode_changed;
        }

        const InputEvents events = read_buttons(
            up_button,
            down_button,
            enter_button,
            gpio_snapshot,
            now_us);

        // A button edge coincident with a real mode change is printed,
        // but not applied to the newly entered default page.
        if (!mode_changed)
        {
            ui_dirty =
                update_current_mode(
                    ui,
                    events,
                    now_us) ||
                ui_dirty;
        }

        ui_dirty =
            update_mock_data(ui, now_us) ||
            ui_dirty;

        if (ui_dirty &&
            ui.current_mode_valid)
        {
            refresh_display(ui);
        }

        sleep_ms(input_poll_time_ms);
    }
}
