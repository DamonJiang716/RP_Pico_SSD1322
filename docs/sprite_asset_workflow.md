# Medusa Sprite Asset Workflow

## 1. Folder structure

- Original PNG, BMP, and JPEG images go into `assets/source/`.
- Generated C++ headers go into `include/generated/`.
- Generated headers should not normally be edited manually.
- `tools/image_to_sprite.py` converts source images into Sprite headers.
- `tools/test_image_to_sprite.py` tests the converter's pure functions.
- This workflow is documented in `docs/sprite_asset_workflow.md`.

For example, the arrow asset uses these paths:

```text
assets/source/test_arrow.png
include/generated/test_arrow.hpp
```

## 2. Image preparation

- PNG is recommended.
- Black or transparent backgrounds are preferred for OLED icons. Transparent
  pixels are composited onto black during conversion.
- Colour images are accepted but are converted to grayscale.
- The final Sprite contains four grayscale levels.
- Simple, high-contrast icons work better than detailed photographs.
- Suggested sizes include 16×16, 24×24, and 32×32 pixels.

## 3. Conversion command

Run the converter from the project root:

```text
python tools/image_to_sprite.py assets/source/cat.png include/generated/test_arrow.hpp --name test_arrow --width 16 --height 16 --preview
```

- `assets/source/test_arrow.png` is the input image path.
- `include/generated/test_arrow.hpp` is the output header path.
- `--name test_arrow` sets the generated C++ resource name.
- `--width 16` sets the output width.
- `--height 16` sets the output height.
- `--preview` prints a four-level terminal preview.

`--width` and `--height` must be supplied together. Resizing uses
nearest-neighbour sampling. Without them, the original image size is kept.

## 4. Generated C++ resource

Include a generated asset with its path relative to the `include/` directory:

```cpp
#include "generated/test_arrow.hpp"
```

The generated Sprite descriptor is then available as:

```cpp
generated_assets::test_arrow
```

The header contains the packed data array, its size, and the Sprite dimensions.
Do not edit the generated hexadecimal array manually.

## 5. Drawing the Sprite

```cpp
graphics::draw_sprite_2bpp(
    20,
    10,
    generated_assets::test_arrow,
    graphics::Rotation::Degrees0);
```

- `x` and `y` are the top-left coordinates of the rotated output.
- The screen origin is at the top-left.
- `x` increases to the right.
- `y` increases downward.
- The supported rotations are 0°, 90°, 180°, and 270° clockwise.

The 2bpp levels 0, 1, 2, and 3 are drawn into the 4bpp framebuffer as 0, 5,
10, and 15. Sprite drawing is opaque, so grayscale level 0 also overwrites the
existing framebuffer pixel.

## 6. Rebuilding

Configure the project when required:

```text
cmake -S . -B build
```

Build the Sprite OLED test firmware:

```text
cmake --build build --target medusa_sprite_oled_test
```

Build the main firmware:

```text
cmake --build build --target medusa_firmware
```

## 7. Testing

- Check the terminal preview produced by `--preview`.
- Check the image orientation, including any required rotations.
- Check all four grayscale levels.
- Check for row misalignment or four-pixel packing errors.
- Flash the correct, newly generated UF2 file when performing hardware tests.

For the Sprite OLED test, also confirm that the four arrows match the `R0`,
`R90`, `R180`, and `R270` labels and that no halt error is printed.

## 8. Updating an image

After changing an original image in `assets/source/`, run the same conversion
command again and rebuild the relevant firmware target. Do not manually edit the
generated hexadecimal array or its metadata.
