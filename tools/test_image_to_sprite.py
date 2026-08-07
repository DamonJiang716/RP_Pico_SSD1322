#!/usr/bin/env python3
"""Pure-function tests for image_to_sprite.py; Pillow is not required."""

from pathlib import Path
import sys
import unittest


sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

from image_to_sprite import (  # noqa: E402
    format_preview,
    generate_cpp_header,
    pack_image_2bpp,
    pack_row_2bpp,
    quantize_grayscale,
    sanitize_cpp_identifier,
)


class ImageToSpriteTests(unittest.TestCase):
    def test_quantize_endpoints(self) -> None:
        self.assertEqual(quantize_grayscale(0), 0)
        self.assertEqual(quantize_grayscale(255), 3)

    def test_quantize_reference_levels(self) -> None:
        values = [0, 85, 170, 255]
        self.assertEqual(
            [quantize_grayscale(value) for value in values],
            [0, 1, 2, 3],
        )

    def test_quantize_boundaries(self) -> None:
        values = [42, 43, 127, 128, 212, 213]
        self.assertEqual(
            [quantize_grayscale(value) for value in values],
            [0, 1, 1, 2, 2, 3],
        )

    def test_quantize_rejects_invalid_values(self) -> None:
        for value in (-1, 256, 1.5, True):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    quantize_grayscale(value)

    def test_pack_four_pixels(self) -> None:
        self.assertEqual(
            pack_row_2bpp([3, 2, 1, 0]),
            bytes([0xE4]),
        )

    def test_pack_five_pixels(self) -> None:
        self.assertEqual(
            pack_row_2bpp([3, 2, 1, 0, 3]),
            bytes([0xE4, 0xC0]),
        )

    def test_rows_are_packed_independently(self) -> None:
        pixels = [
            3, 2, 1, 0, 3,
            0, 1, 2, 3, 1,
        ]
        self.assertEqual(
            pack_image_2bpp(pixels, 5, 2),
            bytes([0xE4, 0xC0, 0x1B, 0x40]),
        )

    def test_odd_width_padding_bits_are_zero(self) -> None:
        self.assertEqual(pack_row_2bpp([3]), bytes([0xC0]))
        self.assertEqual(pack_row_2bpp([1, 2, 3]), bytes([0x6C]))

    def test_pack_rejects_invalid_pixels(self) -> None:
        for row in ([-1], [4], [1.0], [True]):
            with self.subTest(row=row):
                with self.assertRaises(ValueError):
                    pack_row_2bpp(row)

    def test_pack_rejects_wrong_pixel_count(self) -> None:
        with self.assertRaises(ValueError):
            pack_image_2bpp([0, 1, 2], 2, 2)
        with self.assertRaises(ValueError):
            pack_image_2bpp([0, 1, 2, 3, 0], 2, 2)

    def test_pack_rejects_invalid_dimensions(self) -> None:
        for width, height in ((0, 1), (1, 0), (-1, 1), (True, 1)):
            with self.subTest(width=width, height=height):
                with self.assertRaises(ValueError):
                    pack_image_2bpp([], width, height)

    def test_cpp_identifier_sanitizing(self) -> None:
        self.assertEqual(
            sanitize_cpp_identifier("battery_icon"),
            "battery_icon",
        )
        self.assertEqual(
            sanitize_cpp_identifier("battery-icon 2"),
            "battery_icon_2",
        )
        self.assertEqual(
            sanitize_cpp_identifier("123-icon"),
            "sprite_123_icon",
        )
        self.assertEqual(
            sanitize_cpp_identifier("class"),
            "sprite_class",
        )
        self.assertEqual(
            sanitize_cpp_identifier("__icon"),
            "_icon",
        )
        self.assertEqual(
            sanitize_cpp_identifier("asset__icon"),
            "asset_icon",
        )
        self.assertEqual(
            sanitize_cpp_identifier("asset--icon"),
            "asset_icon",
        )
        with self.assertRaises(ValueError):
            sanitize_cpp_identifier("")

    def test_header_generation_is_deterministic(self) -> None:
        arguments = (
            bytes([0x1B, 0xE4]),
            4,
            2,
            "test_icon",
            "test.png",
        )
        self.assertEqual(
            generate_cpp_header(*arguments),
            generate_cpp_header(*arguments),
        )

    def test_header_contains_metadata_and_data(self) -> None:
        header = generate_cpp_header(
            bytes([0x1B, 0xE4]),
            4,
            2,
            "test_icon",
            "C:\\assets\\test.png",
        )
        self.assertIn("// Source image: test.png", header)
        self.assertIn("// Dimensions: 4 x 2", header)
        self.assertIn("// Data bytes: 2", header)
        self.assertIn("0x1B, 0xE4,", header)
        self.assertIn("sizeof(test_icon_data)", header)
        self.assertNotIn("C:\\assets", header)

    def test_header_matches_golden_output(self) -> None:
        expected = """#pragma once

#include "sprite.hpp"

#include <cstddef>
#include <cstdint>

// Source image: test.png
// Dimensions: 4 x 2
// Format: 2bpp grayscale, row-major, 4 pixels per byte.
// Each row starts at a new byte; unused low bits are zero.
// Data bytes: 2

namespace generated_assets
{

inline constexpr std::uint8_t test_icon_data[] =
{
    0x1B, 0xE4,
};

inline constexpr sprite::Sprite2bpp test_icon =
{
    test_icon_data,
    sizeof(test_icon_data),
    4,
    2
};

}
"""
        self.assertEqual(
            generate_cpp_header(
                bytes([0x1B, 0xE4]),
                4,
                2,
                "test_icon",
                "test.png",
            ),
            expected,
        )

    def test_header_rejects_wrong_data_length(self) -> None:
        for data in (bytes([0x1B]), bytes([0x1B, 0xE4, 0x00])):
            with self.subTest(data=data):
                with self.assertRaises(ValueError):
                    generate_cpp_header(
                        data,
                        4,
                        2,
                        "test_icon",
                        "test.png",
                    )

    def test_preview_uses_four_grayscale_characters(self) -> None:
        self.assertEqual(
            format_preview(
                [0, 1, 2, 3, 3, 2, 1, 0],
                4,
                2,
            ),
            " .*#\n#*. ",
        )


if __name__ == "__main__":
    unittest.main()
