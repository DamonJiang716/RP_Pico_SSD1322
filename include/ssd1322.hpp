#pragma once

#include <cstddef>
#include <cstdint>

namespace ssd1322
{
inline constexpr std::size_t display_width = 256;
inline constexpr std::size_t display_height = 64;
inline constexpr std::size_t pixels_per_byte = 2;

inline constexpr std::size_t framebuffer_size =
    display_width * display_height / pixels_per_byte;

static_assert(framebuffer_size == 8192);

enum class Ssd1322DisplayMode : uint8_t
{
    EntireOff = 0xA4,
    EntireOn  = 0xA5,
    Normal    = 0xA6,
    Inverse   = 0xA7
};

void initialise_hardware();
bool initialise_display();
bool write_full_frame(
    const uint8_t* framebuffer,
    std::size_t length);

void ssd1322_hardware_reset();
void ssd1322_set_column_address(uint8_t start, uint8_t end);
void ssd1322_set_row_address(uint8_t start, uint8_t end);
void ssd1322_begin_write_ram();
void ssd1322_set_full_display_window();
void ssd1322_set_remap(uint8_t parameter_a, uint8_t parameter_b);
void ssd1322_set_display_start_line(uint8_t line);
void ssd1322_set_display_offset(uint8_t offset);
void ssd1322_set_display_mode(Ssd1322DisplayMode mode);
void ssd1322_display_off();
void ssd1322_display_on();
void ssd1322_enable_partial_display(uint8_t start_row, uint8_t end_row);
void ssd1322_exit_partial_display();
void ssd1322_select_internal_vdd(bool enable_internal_vdd);
bool ssd1322_set_phase_length(uint8_t phase1_code, uint8_t phase2_code);
bool ssd1322_set_clock(uint8_t oscillator_code, uint8_t divider_code);
void ssd1322_set_display_enhancement_a(
    uint8_t parameter_a,
    uint8_t parameter_b);
void ssd1322_set_controller_gpio(uint8_t configuration);
void ssd1322_set_second_precharge_period(uint8_t dclk_count);
void ssd1322_select_default_grayscale_table();
bool ssd1322_set_custom_grayscale_table(
    const uint8_t* table,
    std::size_t length);
void ssd1322_set_precharge_voltage(uint8_t code);
void ssd1322_set_vcomh_voltage(uint8_t code);
void ssd1322_set_contrast_current(uint8_t contrast);
bool ssd1322_set_master_current_scale(uint8_t scale);
bool ssd1322_set_multiplex_ratio(uint8_t mux_lines);
void ssd1322_set_display_enhancement_b(uint8_t first_parameter);
void ssd1322_set_command_lock(bool lock);
}
