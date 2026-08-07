#pragma once

#include "hardware/spi.h"

namespace board
{
inline spi_inst_t* const oled_spi = spi0;

constexpr uint oled_cs_pin          = 17;
constexpr uint oled_sck_pin         = 18;
constexpr uint oled_sdin_pin        = 19;
constexpr uint oled_res_pin         = 20;
constexpr uint oled_dc_pin          = 21;
constexpr uint oled_spi_baudrate_hz = 1'000'000;
}
