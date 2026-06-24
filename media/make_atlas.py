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


def find_best_layout(icon_count, icon_size, spacing):
    best = None

    for cols in range(1, icon_count + 1):
        rows = math.ceil(icon_count / cols)

        used_w = cols * icon_size + max(0, cols - 1) * spacing
        used_h = rows * icon_size + max(0, rows - 1) * spacing

        atlas_w = next_power_of_two(used_w)
        atlas_h = next_power_of_two(used_h)

        area = atlas_w * atlas_h

        candidate = {
            "cols": cols,
            "rows": rows,
            "used_w": used_w,
            "used_h": used_h,
            "atlas_w": atlas_w,
            "atlas_h": atlas_h,
            "area": area,
        }

        if best is None:
            best = candidate
            continue

        if candidate["area"] < best["area"]:
            best = candidate
            continue

        if candidate["area"] == best["area"]:
            current_aspect_error = abs(candidate["atlas_w"] - candidate["atlas_h"])
            best_aspect_error = abs(best["atlas_w"] - best["atlas_h"])

            if current_aspect_error < best_aspect_error:
                best = candidate

    return best


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
        f.write(" * Icon rects use pixel coordinates with origin at the top left.\n")
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
            x = icon["x"] / atlas_w; 
            y = icon["y"] / atlas_h;
            w = icon["w"] / atlas_w;
            h = icon["h"] / atlas_h;
            f.write(f"  {{ {x}, {y}, {w}, {h} }}, /* {icon['file']} */\n")

        f.write("};\n\n")

        f.write(f"static const unsigned char {png_array_name}[] =\n")
        f.write("{\n")
        f.write(format_c_bytes(png_bytes))
        f.write("\n};\n\n")

        f.write(f"static const unsigned int {png_size_name} = sizeof({png_array_name});\n\n")

        f.write(f"#endif /* {guard} */\n")


def build_atlas(
    input_folder,
    icon_size,
    output_name,
    type_prefix,
    symbol_prefix,
    var_prefix,
):
    input_path = Path(input_folder)
    output_path = Path(output_name)

    if not input_path.exists():
        raise RuntimeError(f"Input folder does not exist: {input_path}")

    if not input_path.is_dir():
        raise RuntimeError(f"Input path is not a folder: {input_path}")

    png_files = sorted(input_path.glob("*.png"))

    if not png_files:
        raise RuntimeError(f"No PNG files found in: {input_path}")

    images = []
    used_names = {}

    for png_file in png_files:
        image = Image.open(png_file).convert("RGBA")

        w, h = image.size

        if w != icon_size or h != icon_size:
            raise RuntimeError(
                f"Invalid icon size for {png_file.name}: "
                f"expected {icon_size}x{icon_size}, got {w}x{h}"
            )

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

    layout = find_best_layout(len(images), icon_size, SPACING)

    atlas_w = layout["atlas_w"]
    atlas_h = layout["atlas_h"]
    cols = layout["cols"]

    atlas = Image.new("RGBA", (atlas_w, atlas_h), (0, 0, 0, 0))

    icons = []

    for index, item in enumerate(images):
        col = index % cols
        row = index // cols

        x = col * (icon_size + SPACING)
        y = row * (icon_size + SPACING)

        atlas.paste(item["image"], (x, y), item["image"])

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

    png_bytes = png_path.read_bytes()

    generate_header(
        header_path=header_path,
        png_bytes=png_bytes,
        icons=icons,
        atlas_w=atlas_w,
        atlas_h=atlas_h,
        type_prefix=type_prefix,
        symbol_prefix=symbol_prefix,
        var_prefix=var_prefix,
    )

    print(f"Generated: {png_path}")
    print(f"Generated: {header_path}")
    print(f"Icons: {len(images)}")
    print(f"Atlas: {atlas_w}x{atlas_h}")
    print(f"Grid: {layout['cols']}x{layout['rows']}")
    print(f"Used area: {layout['used_w']}x{layout['used_h']}")


def main():
    parser = argparse.ArgumentParser(
        description="Build a power-of-two PNG icon atlas and C header."
    )

    parser.add_argument(
        "input_folder",
        help="Folder containing source PNG icons."
    )

    parser.add_argument(
        "icon_size",
        type=int,
        help="Expected icon width/height in pixels, e.g. 16."
    )

    parser.add_argument(
        "output_name",
        help="Output base name. Example: ldk_editor_asset"
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

    args = parser.parse_args()

    build_atlas(
        input_folder=args.input_folder,
        icon_size=args.icon_size,
        output_name=args.output_name,
        type_prefix=args.type_prefix,
        symbol_prefix=args.symbol_prefix,
        var_prefix=args.var_prefix,
    )


if __name__ == "__main__":
    main()
