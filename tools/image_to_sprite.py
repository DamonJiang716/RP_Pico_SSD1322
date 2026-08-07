#!/usr/bin/env python3
"""Convert an image into a deterministic, row-aligned 2bpp Sprite header."""

import argparse
from pathlib import Path
import sys
from typing import Optional, Sequence


PILLOW_ERROR = (
    "Pillow is required for image conversion.\n"
    "Install it with: python -m pip install Pillow"
)


class PillowRequiredError(RuntimeError):
    """Raised when image conversion is requested without Pillow."""


CPP_KEYWORDS = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand",
    "bitor", "bool", "break", "case", "catch", "char", "char8_t",
    "char16_t", "char32_t", "class", "compl", "concept", "const",
    "consteval", "constexpr", "constinit", "const_cast", "continue",
    "co_await", "co_return", "co_yield", "decltype", "default",
    "delete", "do", "double", "dynamic_cast", "else", "enum",
    "explicit", "export", "extern", "false", "float", "for", "friend",
    "goto", "if", "inline", "int", "long", "mutable", "namespace",
    "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or",
    "or_eq", "private", "protected", "public", "register",
    "reinterpret_cast", "requires", "return", "short", "signed",
    "sizeof", "static", "static_assert", "static_cast", "struct",
    "switch", "template", "this", "thread_local", "throw", "true",
    "try", "typedef", "typeid", "typename", "union", "unsigned",
    "using", "virtual", "void", "volatile", "wchar_t", "while", "xor",
    "xor_eq",
}


def _is_integer(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _validate_dimension(value: int, name: str) -> None:
    if not _is_integer(value) or value <= 0:
        raise ValueError(f"{name} must be a positive integer.")


def _validate_quantized_value(value: int) -> None:
    if not _is_integer(value) or value < 0 or value > 3:
        raise ValueError("2bpp pixel values must be integers from 0 to 3.")


def quantize_grayscale(value: int) -> int:
    """Map an 8-bit grayscale value to 0..3 using linear rounding."""
    if not _is_integer(value) or value < 0 or value > 255:
        raise ValueError("Grayscale values must be integers from 0 to 255.")

    # Deterministic nearest-level mapping for 0, 85, 170, and 255.
    return (value * 3 + 127) // 255


def pack_row_2bpp(values: list[int]) -> bytes:
    """Pack one row, placing its first pixel in bits 7..6."""
    packed = bytearray((len(values) + 3) // 4)

    for index, value in enumerate(values):
        _validate_quantized_value(value)
        shift = 6 - 2 * (index % 4)
        packed[index // 4] |= value << shift

    return bytes(packed)


def pack_image_2bpp(
    pixels: list[int],
    width: int,
    height: int,
) -> bytes:
    """Pack an image row by row without carrying unused bits between rows."""
    _validate_dimension(width, "width")
    _validate_dimension(height, "height")

    expected_pixels = width * height
    if len(pixels) != expected_pixels:
        raise ValueError(
            f"Expected {expected_pixels} pixels, received {len(pixels)}."
        )

    packed = bytearray()
    for row in range(height):
        row_begin = row * width
        row_end = row_begin + width
        packed.extend(pack_row_2bpp(pixels[row_begin:row_end]))

    return bytes(packed)


def sanitize_cpp_identifier(name: str) -> str:
    """Convert a requested asset name into a usable C++ identifier."""
    if not isinstance(name, str) or name == "":
        raise ValueError("Sprite name must not be empty.")

    identifier = "".join(
        character
        if (
            "a" <= character <= "z"
            or "A" <= character <= "Z"
            or "0" <= character <= "9"
            or character == "_"
        )
        else "_"
        for character in name
    )

    while "__" in identifier:
        identifier = identifier.replace("__", "_")

    if identifier[0].isdigit():
        identifier = f"sprite_{identifier}"

    if identifier in CPP_KEYWORDS:
        identifier = f"sprite_{identifier}"

    if identifier.startswith("__") or (
        len(identifier) >= 2
        and identifier[0] == "_"
        and identifier[1].isupper()
    ):
        identifier = f"sprite{identifier}"

    return identifier


def format_preview(
    pixels: list[int],
    width: int,
    height: int,
) -> str:
    """Return an ASCII preview using space, '.', '*', and '#'."""
    _validate_dimension(width, "width")
    _validate_dimension(height, "height")

    if len(pixels) != width * height:
        raise ValueError("Preview pixel count does not match its dimensions.")

    characters = " .*#"
    lines = []

    for row in range(height):
        row_characters = []
        for column in range(width):
            value = pixels[row * width + column]
            _validate_quantized_value(value)
            row_characters.append(characters[value])
        lines.append("".join(row_characters))

    return "\n".join(lines)


def _portable_basename(path: str) -> str:
    normalized = str(path).replace("\\", "/")
    basename = normalized.rsplit("/", 1)[-1]
    basename = basename.replace("\r", "_").replace("\n", "_")
    return basename.replace("*/", "*_")


def generate_cpp_header(
    data: bytes,
    width: int,
    height: int,
    name: str,
    source_filename: str,
) -> str:
    """Generate deterministic C++ source for a packed Sprite2bpp resource."""
    _validate_dimension(width, "width")
    _validate_dimension(height, "height")

    if not isinstance(data, (bytes, bytearray)):
        raise ValueError("Sprite data must be bytes.")

    required_size = ((width + 3) // 4) * height
    if len(data) != required_size:
        raise ValueError(
            f"Expected {required_size} data bytes, received {len(data)}."
        )

    identifier = sanitize_cpp_identifier(name)
    source_name = _portable_basename(source_filename)
    byte_values = bytes(data)

    lines = [
        "#pragma once",
        "",
        '#include "sprite.hpp"',
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        f"// Source image: {source_name}",
        f"// Dimensions: {width} x {height}",
        "// Format: 2bpp grayscale, row-major, 4 pixels per byte.",
        "// Each row starts at a new byte; unused low bits are zero.",
        f"// Data bytes: {len(byte_values)}",
        "",
        "namespace generated_assets",
        "{",
        "",
        f"inline constexpr std::uint8_t {identifier}_data[] =",
        "{",
    ]

    bytes_per_line = 12
    for start in range(0, len(byte_values), bytes_per_line):
        chunk = byte_values[start:start + bytes_per_line]
        formatted = ", ".join(f"0x{value:02X}" for value in chunk)
        lines.append(f"    {formatted},")

    lines.extend([
        "};",
        "",
        f"inline constexpr sprite::Sprite2bpp {identifier} =",
        "{",
        f"    {identifier}_data,",
        f"    sizeof({identifier}_data),",
        f"    {width},",
        f"    {height}",
        "};",
        "",
        "}",
        "",
    ])

    return "\n".join(lines)


def load_and_quantize_image(
    input_path: Path,
    width: Optional[int] = None,
    height: Optional[int] = None,
) -> tuple[list[int], int, int]:
    """Load with Pillow, composite alpha on black, resize, and quantize."""
    if (width is None) != (height is None):
        raise ValueError("--width and --height must be provided together.")

    if width is not None and height is not None:
        _validate_dimension(width, "width")
        _validate_dimension(height, "height")

    try:
        from PIL import Image
    except ImportError as error:
        raise PillowRequiredError(PILLOW_ERROR) from error

    with Image.open(input_path) as source_image:
        rgba_image = source_image.convert("RGBA")
        black_background = Image.new(
            "RGBA",
            rgba_image.size,
            (0, 0, 0, 255),
        )
        composited = Image.alpha_composite(
            black_background,
            rgba_image,
        )
        grayscale_image = composited.convert("L")

        if width is not None and height is not None:
            resampling = getattr(Image, "Resampling", Image)
            grayscale_image = grayscale_image.resize(
                (width, height),
                resampling.NEAREST,
            )

        image_width, image_height = grayscale_image.size
        pixels = [
            quantize_grayscale(value)
            for value in grayscale_image.getdata()
        ]

    return pixels, image_width, image_height


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Convert an image to a row-aligned 2bpp grayscale C++ Sprite."
        )
    )
    parser.add_argument("input", type=Path, help="Input image file")
    parser.add_argument("output", type=Path, help="Output C++ header")
    parser.add_argument("--name", required=True, help="C++ asset name")
    parser.add_argument("--width", type=int, help="Output width")
    parser.add_argument("--height", type=int, help="Output height")
    parser.add_argument(
        "--preview",
        action="store_true",
        help="Print a four-level ASCII preview",
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_argument_parser()
    arguments = parser.parse_args(argv)

    if (arguments.width is None) != (arguments.height is None):
        parser.error("--width and --height must be provided together.")

    try:
        sanitize_cpp_identifier(arguments.name)
        pixels, width, height = load_and_quantize_image(
            arguments.input,
            arguments.width,
            arguments.height,
        )
        packed = pack_image_2bpp(pixels, width, height)
        header = generate_cpp_header(
            packed,
            width,
            height,
            arguments.name,
            str(arguments.input),
        )

        with arguments.output.open(
            "w",
            encoding="utf-8",
            newline="\n",
        ) as output_file:
            output_file.write(header)
    except PillowRequiredError:
        print(PILLOW_ERROR, file=sys.stderr)
        return 1
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1

    if arguments.preview:
        print("----- SPRITE PREVIEW -----")
        print(format_preview(pixels, width, height))
        print("----- END SPRITE PREVIEW -----")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
