#!/usr/bin/env python3

import argparse
import math
import re
from pathlib import Path
from PIL import Image

SPACING = 2


def next_power_of_two(value):
    if value <= 1:
        return 1
    return 1 << (value - 1).bit_length()


def sanitize_c_identifier(name):
    name = Path(name).stem
    name = re.sub(r"[^a-zA-Z0-9]+", "_", name)
    name = re.sub(r"_+", "_", name)
    name = name.strip("_")
    name = name.upper()

    if not name:
        name = "ICON"

    if name[0].isdigit():
        name = "_" + name

    return name


def pack_images(images, atlas_w, atlas_h, spacing):
    # Each rectangle reserves spacing on its right and bottom edges.
    # The extra space in the packing area removes padding at the atlas edges.
    free_rects = [(0, 0, atlas_w + spacing, atlas_h + spacing)]
    positions = {}

    order = sorted(
        range(len(images)),
        key=lambda i: (
            -max(images[i]["w"], images[i]["h"]),
            -(images[i]["w"] * images[i]["h"]),
            images[i]["name"],
        ),
    )

    for index in order:
        width = images[index]["w"] + spacing
        height = images[index]["h"] + spacing
        best = None

        for x, y, w, h in free_rects:
            if width > w or height > h:
                continue

            score = (min(w - width, h - height), max(w - width, h - height), y, x)
            if best is None or score < best[0]:
                best = (score, x, y)

        if best is None:
            return None

        _, placed_x, placed_y = best
        positions[index] = (placed_x, placed_y)
        used_right = placed_x + width
        used_bottom = placed_y + height
        remaining = []

        for x, y, w, h in free_rects:
            right = x + w
            bottom = y + h

            if (placed_x >= right or used_right <= x or
                    placed_y >= bottom or used_bottom <= y):
                remaining.append((x, y, w, h))
                continue

            if placed_x > x:
                remaining.append((x, y, placed_x - x, h))
            if used_right < right:
                remaining.append((used_right, y, right - used_right, h))
            if placed_y > y:
                remaining.append((x, y, w, placed_y - y))
            if used_bottom < bottom:
                remaining.append((x, used_bottom, w, bottom - used_bottom))

        # Free rectangles may overlap; discard those contained in another.
        free_rects = []
        for i, (x, y, w, h) in enumerate(remaining):
            contained = any(
                i != j and
                other_x <= x and other_y <= y and
                x + w <= other_x + other_w and
                y + h <= other_y + other_h
                for j, (other_x, other_y, other_w, other_h) in enumerate(remaining)
            )
            if not contained:
                free_rects.append((x, y, w, h))

    return positions


def find_best_layout(images, spacing):
    min_width = next_power_of_two(max(item["w"] for item in images))
    min_height = next_power_of_two(max(item["h"] for item in images))
    max_width = next_power_of_two(sum(item["w"] for item in images) +
                                  spacing * (len(images) - 1))
    max_height = next_power_of_two(sum(item["h"] for item in images) +
                                   spacing * (len(images) - 1))
    min_area = sum(item["w"] * item["h"] for item in images)
    area_limit = min(max_width * min_height, min_width * max_height)

    candidates = []
    width = min_width
    while width <= max_width and width * min_height <= area_limit:
        height = min_height
        while height <= max_height and width * height <= area_limit:
            area = width * height
            if area >= min_area:
                candidates.append((area, abs(width - height), width, height))
            height *= 2
        width *= 2

    # Prefer smaller power-of-two atlases and, at equal area, squarer ones.
    for _, _, atlas_w, atlas_h in sorted(candidates):
        positions = pack_images(images, atlas_w, atlas_h, spacing)
        if positions is not None:
            return {
                "atlas_w": atlas_w,
                "atlas_h": atlas_h,
                "positions": positions,
            }

    raise RuntimeError("Unable to pack images into an atlas")


def format_c_bytes(data):
    lines = []

    for i in range(0, len(data), 12):
        chunk = data[i:i + 12]
        values = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append("  " + values + ",")

    return "\n".join(lines)


def make_header_guard(filename):
    guard = sanitize_c_identifier(filename)
    return f"{guard}_H"


def generate_header(
    header_path,
    png_bytes,
    icons,
    atlas_w,
    atlas_h,
    type_prefix,
    symbol_prefix,
    var_prefix,
    embed_png,
):
    guard = make_header_guard(header_path.name)

    enum_type = f"{type_prefix}Icon"
    rect_type = f"{type_prefix}IconRect"

    enum_count_name = f"{symbol_prefix}_ICON_COUNT"
    rect_array_name = f"{var_prefix}_icon_rects"
    png_array_name = f"{var_prefix}_icon_atlas_png"
    png_size_name = f"{var_prefix}_icon_atlas_png_size"
    atlas_width_name = f"{symbol_prefix}_ICON_ATLAS_WIDTH"
    atlas_height_name = f"{symbol_prefix}_ICON_ATLAS_HEIGHT"

    with open(header_path, "w", encoding="utf-8") as f:
        f.write("/*\n")
        f.write(" * This file is auto generated. Do not edit manually.\n")
        f.write(" * Icon rects use normalized coordinates with origin at the top left.\n")
        f.write(" */\n")
        f.write(f"#ifndef {guard}\n")
        f.write(f"#define {guard}\n\n")

        f.write("#include <stdint.h>\n\n")
        f.write("#include <ldk_geom.h>\n\n")
        f.write(f"typedef LDKRectf {rect_type};\n\n")

        f.write(f"#define {atlas_width_name} {atlas_w}u\n")
        f.write(f"#define {atlas_height_name} {atlas_h}u\n\n")

        f.write(f"typedef enum {enum_type}\n")
        f.write("{\n")

        for icon in icons:
            f.write(f"  {symbol_prefix}_ICON_{icon['name']},\n")

        f.write("\n")
        f.write(f"  {enum_count_name}\n")
        f.write(f"}} {enum_type};\n\n")

        f.write(f"static const {rect_type} {rect_array_name}[{enum_count_name}] =\n")
        f.write("{\n")

        for icon in icons:
            x = icon["x"] / atlas_w
            y = icon["y"] / atlas_h
            w = icon["w"] / atlas_w
            h = icon["h"] / atlas_h
            f.write(f"  {{ {x}, {y}, {w}, {h} }}, /* {icon['file']} */\n")

        f.write("};\n\n")

        if embed_png:
            f.write(f"static const unsigned char {png_array_name}[] =\n")
            f.write("{\n")
            f.write(format_c_bytes(png_bytes))
            f.write("\n};\n\n")

            f.write(f"static const unsigned int {png_size_name} = sizeof({png_array_name});\n\n")

        f.write(f"#endif /* {guard} */\n")


def build_atlas(
    input_folder,
    output_name,
    max_size,
    type_prefix,
    symbol_prefix,
    var_prefix,
    embed_png,
):
    input_path = Path(input_folder)
    output_path = Path(output_name)

    if not input_path.exists():
        raise RuntimeError(f"Input folder does not exist: {input_path}")

    if not input_path.is_dir():
        raise RuntimeError(f"Input path is not a folder: {input_path}")

    if max_size is not None and max_size <= 0:
        raise RuntimeError("Maximum image size must be positive")

    png_files = sorted(input_path.glob("*.png"))

    if not png_files:
        raise RuntimeError(f"No PNG files found in: {input_path}")

    images = []
    used_names = {}

    for png_file in png_files:
        with Image.open(png_file) as source:
            image = source.convert("RGBA")

        if max_size is not None:
            image.thumbnail((max_size, max_size), Image.Resampling.LANCZOS)

        w, h = image.size
        name = sanitize_c_identifier(png_file.name)

        if name in used_names:
            raise RuntimeError(
                f"Duplicate enum name generated: {name} "
                f"from {used_names[name]} and {png_file.name}"
            )

        used_names[name] = png_file.name

        images.append({
            "file": png_file.name,
            "path": png_file,
            "name": name,
            "image": image,
            "w": w,
            "h": h,
        })

    layout = find_best_layout(images, SPACING)
    atlas_w = layout["atlas_w"]
    atlas_h = layout["atlas_h"]

    atlas = Image.new("RGBA", (atlas_w, atlas_h), (0, 0, 0, 0))
    icons = []

    for index, item in enumerate(images):
        x, y = layout["positions"][index]
        # Do not pass an alpha mask: the source image already has alpha.
        atlas.paste(item["image"], (x, y))

        icons.append({
            "file": item["file"],
            "name": item["name"],
            "x": x,
            "y": y,
            "w": item["w"],
            "h": item["h"],
        })

    png_path = output_path.with_suffix(".png")
    header_path = output_path.with_suffix(".h")

    atlas.save(png_path, format="PNG")

    png_bytes = png_path.read_bytes() if embed_png else None

    generate_header(
        header_path=header_path,
        png_bytes=png_bytes,
        icons=icons,
        atlas_w=atlas_w,
        atlas_h=atlas_h,
        type_prefix=type_prefix,
        symbol_prefix=symbol_prefix,
        var_prefix=var_prefix,
        embed_png=embed_png,
    )

    used_w = max(icon["x"] + icon["w"] for icon in icons)
    used_h = max(icon["y"] + icon["h"] for icon in icons)

    print(f"Generated: {png_path}")
    print(f"Generated: {header_path}")
    print(f"Icons: {len(images)}")
    print(f"Atlas: {atlas_w}x{atlas_h}")
    print(f"Used area: {used_w}x{used_h}")


def main():
    parser = argparse.ArgumentParser(
        description="Build a power-of-two PNG icon atlas and C header."
    )

    parser.add_argument(
        "input_folder",
        help="Folder containing source PNG icons."
    )

    parser.add_argument(
        "output_name",
        help="Output base name. Example: ldk_editor_asset"
    )

    parser.add_argument(
        "max_size",
        type=int,
        nargs="?",
        default=None,
        help="Optional maximum width/height in pixels. Larger images are scaled down proportionally."
    )

    parser.add_argument(
        "--type-prefix",
        default="LDKEditor",
        help="C type prefix. Example: LDKEditor -> LDKEditorIcon"
    )

    parser.add_argument(
        "--symbol-prefix",
        default="LDK_EDITOR",
        help="C macro/enum prefix. Example: LDK_EDITOR -> LDK_EDITOR_ICON_PLAY"
    )

    parser.add_argument(
        "--var-prefix",
        default="ldk_editor",
        help="C variable prefix. Example: ldk_editor -> ldk_editor_icon_rects"
    )

    parser.add_argument(
        "--no-embed-png",
        action="store_true",
        help="Do not embed the generated PNG bytes in the C header."
    )

    args = parser.parse_args()

    build_atlas(
        input_folder=args.input_folder,
        output_name=args.output_name,
        max_size=args.max_size,
        type_prefix=args.type_prefix,
        symbol_prefix=args.symbol_prefix,
        var_prefix=args.var_prefix,
        embed_png=not args.no_embed_png,
    )


if __name__ == "__main__":
    main()
