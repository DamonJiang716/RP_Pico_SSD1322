#include "ssd1322.hpp"
#include "board_config.hpp"

#include "pico/stdlib.h"
#include "hardware/spi.h"

#include <cstddef>
#include <cstdint>

namespace
{
// ============================================================
// SSD1322 command codes
// ============================================================

constexpr uint8_t SSD1322_CMD_ENABLE_GRAYSCALE_TABLE = 0x00;
constexpr uint8_t SSD1322_CMD_SET_COLUMN_ADDRESS     = 0x15;
constexpr uint8_t SSD1322_CMD_WRITE_RAM              = 0x5C;
constexpr uint8_t SSD1322_CMD_SET_ROW_ADDRESS        = 0x75;
constexpr uint8_t SSD1322_CMD_SET_REMAP               = 0xA0;
constexpr uint8_t SSD1322_CMD_SET_START_LINE          = 0xA1;
constexpr uint8_t SSD1322_CMD_SET_DISPLAY_OFFSET      = 0xA2;

constexpr uint8_t SSD1322_CMD_ENTIRE_DISPLAY_OFF      = 0xA4;
constexpr uint8_t SSD1322_CMD_ENTIRE_DISPLAY_ON       = 0xA5;
constexpr uint8_t SSD1322_CMD_NORMAL_DISPLAY          = 0xA6;
constexpr uint8_t SSD1322_CMD_INVERSE_DISPLAY         = 0xA7;

constexpr uint8_t SSD1322_CMD_ENABLE_PARTIAL_DISPLAY  = 0xA8;
constexpr uint8_t SSD1322_CMD_EXIT_PARTIAL_DISPLAY    = 0xA9;
constexpr uint8_t SSD1322_CMD_FUNCTION_SELECTION      = 0xAB;

constexpr uint8_t SSD1322_CMD_DISPLAY_OFF             = 0xAE;
constexpr uint8_t SSD1322_CMD_DISPLAY_ON              = 0xAF;

constexpr uint8_t SSD1322_CMD_SET_PHASE_LENGTH        = 0xB1;
constexpr uint8_t SSD1322_CMD_SET_CLOCK               = 0xB3;
constexpr uint8_t SSD1322_CMD_ENHANCEMENT_A           = 0xB4;
constexpr uint8_t SSD1322_CMD_SET_GPIO                = 0xB5;
constexpr uint8_t SSD1322_CMD_SECOND_PRECHARGE        = 0xB6;
constexpr uint8_t SSD1322_CMD_SET_GRAYSCALE_TABLE     = 0xB8;
constexpr uint8_t SSD1322_CMD_DEFAULT_GRAYSCALE       = 0xB9;
constexpr uint8_t SSD1322_CMD_PRECHARGE_VOLTAGE       = 0xBB;
constexpr uint8_t SSD1322_CMD_VCOMH_VOLTAGE           = 0xBE;
constexpr uint8_t SSD1322_CMD_CONTRAST_CURRENT        = 0xC1;
constexpr uint8_t SSD1322_CMD_MASTER_CURRENT          = 0xC7;
constexpr uint8_t SSD1322_CMD_MULTIPLEX_RATIO         = 0xCA;
constexpr uint8_t SSD1322_CMD_ENHANCEMENT_B           = 0xD1;
constexpr uint8_t SSD1322_CMD_COMMAND_LOCK            = 0xFD;


// ============================================================
// Low-level transfer helpers
// ============================================================

void ssd1322_write_command(uint8_t command)
{
    // D/C# LOW: the following byte is a command.
    gpio_put(board::oled_dc_pin, 0);

    // CS# LOW: select the SSD1322.
    gpio_put(board::oled_cs_pin, 0);

    // Send one command byte through SPI0.
    spi_write_blocking(board::oled_spi, &command, 1);

    // CS# HIGH: finish the transfer.
    gpio_put(board::oled_cs_pin, 1);
}


void ssd1322_write_data(uint8_t data)
{
    // D/C# HIGH: the following byte is data or a command parameter.
    gpio_put(board::oled_dc_pin, 1);

    gpio_put(board::oled_cs_pin, 0);

    spi_write_blocking(board::oled_spi, &data, 1);

    gpio_put(board::oled_cs_pin, 1);
}


void ssd1322_write_data_buffer(const uint8_t* data, std::size_t length)
{
    // Ignore an empty or invalid buffer.
    if (data == nullptr || length == 0)
    {
        return;
    }

    // Keep CS# LOW while sending the complete buffer.
    gpio_put(board::oled_dc_pin, 1);
    gpio_put(board::oled_cs_pin, 0);

    spi_write_blocking(board::oled_spi, data, length);

    gpio_put(board::oled_cs_pin, 1);
}
}

namespace ssd1322
{
void initialise_hardware()
{
    // Initialise SPI0 at 1 MHz.
    spi_init(board::oled_spi, board::oled_spi_baudrate_hz);

    // SSD1322 uses 8-bit, Mode 0, MSB-first SPI.
    spi_set_format(
        board::oled_spi,
        8,
        SPI_CPOL_0,
        SPI_CPHA_0,
        SPI_MSB_FIRST
    );

    // Hardware SPI pins.
    gpio_set_function(board::oled_sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(board::oled_sdin_pin, GPIO_FUNC_SPI);

    // Chip select: inactive HIGH.
    gpio_init(board::oled_cs_pin);
    gpio_set_dir(board::oled_cs_pin, GPIO_OUT);
    gpio_put(board::oled_cs_pin, 1);

    // Reset: inactive HIGH.
    gpio_init(board::oled_res_pin);
    gpio_set_dir(board::oled_res_pin, GPIO_OUT);
    gpio_put(board::oled_res_pin, 1);

    // Data/Command output.
    gpio_init(board::oled_dc_pin);
    gpio_set_dir(board::oled_dc_pin, GPIO_OUT);
    gpio_put(board::oled_dc_pin, 0);
}


bool initialise_display()
{
    ssd1322_hardware_reset();

    // FD 12 — unlock commands.
    ssd1322_set_command_lock(false);

    // B3 D1.
    if (!ssd1322_set_clock(0x0D, 0x01))
    {
        return false;
    }

    // CA 3F.
    // Public interface accepts physical line count,
    // therefore 64 is converted internally to 0x3F.
    if (!ssd1322_set_multiplex_ratio(64))
    {
        return false;
    }

    // A2 00.
    ssd1322_set_display_offset(0x00);

    // A1 00.
    ssd1322_set_display_start_line(0x00);

    // A0 14 11.
    ssd1322_set_remap(0x14, 0x11);

    // B5 00.
    ssd1322_set_controller_gpio(0x00);

    // AB 00 — external VDD.
    ssd1322_select_internal_vdd(false);

    // B4 A0 B5.
    ssd1322_set_display_enhancement_a(0xA0, 0xB5);

    // C1 9F.
    ssd1322_set_contrast_current(0x9F);

    // C7 0F.
    // Public interface accepts scale 1 through 16,
    // therefore 16 is converted internally to 0x0F.
    if (!ssd1322_set_master_current_scale(16))
    {
        return false;
    }

    // B9.
    ssd1322_select_default_grayscale_table();

    // B1 74.
    // phase2 is the high nibble and phase1 is the low nibble.
    if (!ssd1322_set_phase_length(0x04, 0x07))
    {
        return false;
    }

    // D1 A2 20.
    ssd1322_set_display_enhancement_b(0xA2);

    // BB 17.
    ssd1322_set_precharge_voltage(0x17);

    // B6 08.
    ssd1322_set_second_precharge_period(0x08);

    // BE 07.
    ssd1322_set_vcomh_voltage(0x07);

    // A6.
    ssd1322_set_display_mode(
        Ssd1322DisplayMode::Normal);

    // A9.
    ssd1322_exit_partial_display();

    // AF.
    ssd1322_display_on();

    // Newhaven reference waits briefly after OLED_Init.
    sleep_ms(10);


    return true;
}


// ============================================================
// Hardware reset （8.9 Power ON and OFF sequence）
// ============================================================

void ssd1322_hardware_reset()
{
    // RES# LOW resets the SSD1322.
    gpio_put(board::oled_res_pin, 0);
    // The datasheet recommends at least 100 us during power-on.
    sleep_ms(150);

    // RES# HIGH returns the controller to normal operation.
    gpio_put(board::oled_res_pin, 1);
    sleep_ms(150);
}


// ============================================================
// Address and RAM commands （Table 9-1 : Command table）
// ============================================================

void ssd1322_set_column_address(uint8_t start, uint8_t end)
{
    // Valid SSD1322 RAM column range: 0 to 119.
    ssd1322_write_command(SSD1322_CMD_SET_COLUMN_ADDRESS);
    ssd1322_write_data(start);
    ssd1322_write_data(end);
}


void ssd1322_set_row_address(uint8_t start, uint8_t end)
{
    // Valid SSD1322 RAM row range: 0 to 127.
    ssd1322_write_command(SSD1322_CMD_SET_ROW_ADDRESS);
    ssd1322_write_data(start);
    ssd1322_write_data(end);
}


void ssd1322_begin_write_ram()
{
    // All following data bytes are written into GDDRAM until
    // another command is received.
    ssd1322_write_command(SSD1322_CMD_WRITE_RAM);
}


// NHD-2.8-25664UC visible panel window.
// NHD-2.8-25664UC 可见显示区域。
void ssd1322_set_full_display_window()
{
    // The SSD1322 has wider internal RAM than this 256-pixel panel.
    // This Newhaven panel uses RAM columns 0x1C through 0x5B.
    //
    // SSD1322 内部显存宽度大于这块 256 像素屏幕。
    // Newhaven 模块使用 0x1C 到 0x5B 这一段列地址。
    ssd1322_set_column_address(0x1C, 0x5B);

    // 64 visible rows: row 0 through row 63.
    // 64 个可见行：0 到 63。
    ssd1322_set_row_address(0x00, 0x3F);
}


bool write_full_frame(
    const uint8_t* framebuffer,
    std::size_t length)
{
    if (framebuffer == nullptr ||
        length != framebuffer_size)
    {
        return false;
    }

    ssd1322_set_full_display_window();
    ssd1322_begin_write_ram();

    ssd1322_write_data_buffer(
        framebuffer,
        length);

    return true;
}


// ============================================================
// Remap, orientation and display positioning
// 重映射、方向与显示位置
// ============================================================

void ssd1322_set_remap(uint8_t parameter_a, uint8_t parameter_b)
{
    // parameter_a controls:
    // A[0] address increment direction
    // A[1] column remap
    // A[2] nibble remap
    // A[4] COM scan direction
    // A[5] COM split odd/even
    //
    // parameter_a 控制：
    // A[0] 地址递增方向
    // A[1] 列重映射
    // A[2] 半字节重映射
    // A[4] COM 扫描方向
    // A[5] COM 奇偶分离

    // parameter_b bit 4 controls Dual COM mode.
    // parameter_b 的 bit 4 控制 Dual COM 模式。
    ssd1322_write_command(SSD1322_CMD_SET_REMAP);
    ssd1322_write_data(parameter_a);
    ssd1322_write_data(parameter_b);
}


void ssd1322_set_display_start_line(uint8_t line)
{
    // Valid range: 0 to 127.
    // 有效范围：0 到 127。
    ssd1322_write_command(SSD1322_CMD_SET_START_LINE);
    ssd1322_write_data(line & 0x7F);
}


void ssd1322_set_display_offset(uint8_t offset)
{
    // Valid range: 0 to 127.
    // 有效范围：0 到 127。
    ssd1322_write_command(SSD1322_CMD_SET_DISPLAY_OFFSET);
    ssd1322_write_data(offset & 0x7F);
}


// ============================================================
// Display modes
// 显示模式
// ============================================================

void ssd1322_set_display_mode(Ssd1322DisplayMode mode)
{
    ssd1322_write_command(static_cast<uint8_t>(mode));
}


void ssd1322_display_off()
{
    // AEh: sleep mode ON and display circuits OFF.
    // AEh：进入休眠模式并关闭显示驱动电路。
    ssd1322_write_command(SSD1322_CMD_DISPLAY_OFF);
}


void ssd1322_display_on()
{
    // AFh: sleep mode OFF and display circuits ON.
    // AFh：退出休眠模式并打开显示驱动电路。
    ssd1322_write_command(SSD1322_CMD_DISPLAY_ON);
}


// ============================================================
// Partial display — optional
// 局部显示——可选功能
// ============================================================

void ssd1322_enable_partial_display(uint8_t start_row, uint8_t end_row)
{
    // end_row must be greater than or equal to start_row.
    // 结束行必须大于或等于起始行。
    if (start_row > 0x7F || end_row > 0x7F || end_row < start_row)
    {
        return;
    }

    ssd1322_write_command(SSD1322_CMD_ENABLE_PARTIAL_DISPLAY);
    ssd1322_write_data(start_row);
    ssd1322_write_data(end_row);
}


void ssd1322_exit_partial_display()
{
    ssd1322_write_command(SSD1322_CMD_EXIT_PARTIAL_DISPLAY);
}


// ============================================================
// Power and timing configuration
// 电源与时序配置
// ============================================================

void ssd1322_select_internal_vdd(bool enable_internal_vdd)
{
    // 0x01: use internal VDD regulator.
    // 0x00: use external VDD.
    //
    // 0x01：启用内部 VDD 稳压器。
    // 0x00：使用外部 VDD。
    ssd1322_write_command(SSD1322_CMD_FUNCTION_SELECTION);
    ssd1322_write_data(enable_internal_vdd ? 0x01 : 0x00);
}


bool ssd1322_set_phase_length(
    uint8_t phase1_code,
    uint8_t phase2_code)
{
    // Phase 1 code must be 0x2 to 0xF.
    // Phase 2 code must be 0x3 to 0xF.
    //
    // Phase 1 编码必须在 0x2 到 0xF。
    // Phase 2 编码必须在 0x3 到 0xF。
    if (phase1_code < 0x02 || phase1_code > 0x0F ||
        phase2_code < 0x03 || phase2_code > 0x0F)
    {
        return false;
    }

    // Phase 2 occupies the high nibble.
    // Phase 1 occupies the low nibble.
    //
    // Phase 2 位于高四位。
    // Phase 1 位于低四位。
    const uint8_t parameter =
        static_cast<uint8_t>((phase2_code << 4) | phase1_code);

    ssd1322_write_command(SSD1322_CMD_SET_PHASE_LENGTH);
    ssd1322_write_data(parameter);

    return true;
}


bool ssd1322_set_clock(
    uint8_t oscillator_code,
    uint8_t divider_code)
{
    // Oscillator code: 0x0 to 0xF.
    // Divider code: 0x0 to 0xA.
    // Divider codes 0xB to 0xF are invalid.
    //
    // 振荡器编码：0x0 到 0xF。
    // 分频编码：0x0 到 0xA。
    // 分频编码 0xB 到 0xF 无效。
    if (oscillator_code > 0x0F ||
        divider_code > 0x0A)
    {
        return false;
    }

    const uint8_t parameter =
        static_cast<uint8_t>((oscillator_code << 4) | divider_code);

    ssd1322_write_command(SSD1322_CMD_SET_CLOCK);
    ssd1322_write_data(parameter);

    return true;
}


void ssd1322_set_display_enhancement_a(
    uint8_t parameter_a,
    uint8_t parameter_b)
{
    // B4h is called Display Enhancement A in the datasheet.
    // It controls internal/external VSL and low-gray performance.
    //
    // 数据手册将 B4h 称为 Display Enhancement A。
    // 它控制内部/外部 VSL 以及低灰阶表现。
    ssd1322_write_command(SSD1322_CMD_ENHANCEMENT_A);
    ssd1322_write_data(parameter_a);
    ssd1322_write_data(parameter_b);
}


void ssd1322_set_controller_gpio(uint8_t configuration)
{
    // SSD1322 GPIO0/GPIO1 are not normally used on this module.
    // This function is retained because the Newhaven initialization
    // explicitly writes 0x00.
    //
    // 本模块通常不使用 SSD1322 的 GPIO0/GPIO1。
    // 保留该函数是因为 Newhaven 初始化序列明确写入了 0x00。
    ssd1322_write_command(SSD1322_CMD_SET_GPIO);
    ssd1322_write_data(configuration & 0x0F);
}


void ssd1322_set_second_precharge_period(uint8_t dclk_count)
{
    // Valid range: 0 to 15 DCLKs.
    // 有效范围：0 到 15 个 DCLK。
    ssd1322_write_command(SSD1322_CMD_SECOND_PRECHARGE);
    ssd1322_write_data(dclk_count & 0x0F);
}


// ============================================================
// Grayscale table
// 灰度表
// ============================================================

void ssd1322_select_default_grayscale_table()
{
    // B9h is a single-byte command and requires no parameter.
    // B9h 是单字节命令，不需要后续参数。
    ssd1322_write_command(SSD1322_CMD_DEFAULT_GRAYSCALE);
}


bool ssd1322_set_custom_grayscale_table(
    const uint8_t* table,
    std::size_t length)
{
    // The command requires exactly 15 entries: GS1 through GS15.
    // 命令必须包含 15 个数值：GS1 到 GS15。
    if (table == nullptr || length != 15)
    {
        return false;
    }

    // Every value must be no greater than 180,
    // and the table must be strictly increasing.
    //
    // 每个值不得超过 180，
    // 并且各灰阶值必须严格递增。
    for (std::size_t i = 0; i < length; ++i)
    {
        if (table[i] > 180)
        {
            return false;
        }

        if (i > 0 && table[i] <= table[i - 1])
        {
            return false;
        }
    }

    ssd1322_write_command(SSD1322_CMD_SET_GRAYSCALE_TABLE);
    ssd1322_write_data_buffer(table, length);

    // The datasheet requires command 00h after B8h
    // to activate the custom grayscale table.
    //
    // 数据手册要求在 B8h 数据之后发送 00h，
    // 才能启用自定义灰度表。
    ssd1322_write_command(SSD1322_CMD_ENABLE_GRAYSCALE_TABLE);

    return true;
}


// ============================================================
// Voltage and current settings
// 电压与电流设置
// ============================================================

void ssd1322_set_precharge_voltage(uint8_t code)
{
    // Valid code range: 0x00 to 0x1F.
    // 0x00 represents approximately 0.20 × VCC.
    // 0x1F represents approximately 0.60 × VCC.
    //
    // 有效编码：0x00 到 0x1F。
    // 0x00 约为 0.20 × VCC。
    // 0x1F 约为 0.60 × VCC。
    ssd1322_write_command(SSD1322_CMD_PRECHARGE_VOLTAGE);
    ssd1322_write_data(code & 0x1F);
}


void ssd1322_set_vcomh_voltage(uint8_t code)
{
    // Valid code range: 0x00 to 0x07.
    // 0x00 = approximately 0.72 × VCC.
    // 0x07 = approximately 0.86 × VCC.
    //
    // 有效编码：0x00 到 0x07。
    // 0x00 约为 0.72 × VCC。
    // 0x07 约为 0.86 × VCC。
    ssd1322_write_command(SSD1322_CMD_VCOMH_VOLTAGE);
    ssd1322_write_data(code & 0x07);
}


void ssd1322_set_contrast_current(uint8_t contrast)
{
    // Full 8-bit range: 0x00 to 0xFF.
    // Higher values produce greater segment current.
    //
    // 完整 8 位范围：0x00 到 0xFF。
    // 数值越高，段输出电流越大。
    ssd1322_write_command(SSD1322_CMD_CONTRAST_CURRENT);
    ssd1322_write_data(contrast);
}


bool ssd1322_set_master_current_scale(uint8_t scale)
{
    // Human-friendly range: 1 to 16.
    // Register encoding: 0 means 1/16, 15 means 16/16.
    //
    // 对外使用范围：1 到 16。
    // 寄存器编码：0 表示 1/16，15 表示 16/16。
    if (scale < 1 || scale > 16)
    {
        return false;
    }

    const uint8_t register_value =
        static_cast<uint8_t>(scale - 1);

    ssd1322_write_command(SSD1322_CMD_MASTER_CURRENT);
    ssd1322_write_data(register_value);

    return true;
}


bool ssd1322_set_multiplex_ratio(uint8_t mux_lines)
{
    // Valid physical multiplex ratio: 16 to 128.
    // Register value is multiplex ratio minus one.
    //
    // 有效物理复用比：16 到 128。
    // 寄存器值等于复用行数减一。
    if (mux_lines < 16 || mux_lines > 128)
    {
        return false;
    }

    const uint8_t register_value =
        static_cast<uint8_t>(mux_lines - 1);

    ssd1322_write_command(SSD1322_CMD_MULTIPLEX_RATIO);
    ssd1322_write_data(register_value);

    return true;
}


void ssd1322_set_display_enhancement_b(uint8_t first_parameter)
{
    // The second parameter is fixed at 0x20 by the command format.
    // 命令格式规定第二个参数为 0x20。
    ssd1322_write_command(SSD1322_CMD_ENHANCEMENT_B);
    ssd1322_write_data(first_parameter);
    ssd1322_write_data(0x20);
}


// ============================================================
// Command lock
// 命令锁
// ============================================================

void ssd1322_set_command_lock(bool lock)
{
    // 0x12: unlock command and memory access.
    // 0x16: lock all commands and memory access except FDh.
    //
    // 0x12：解锁命令和显存访问。
    // 0x16：锁定除 FDh 之外的命令和显存访问。
    ssd1322_write_command(SSD1322_CMD_COMMAND_LOCK);
    ssd1322_write_data(lock ? 0x16 : 0x12);
}
}
