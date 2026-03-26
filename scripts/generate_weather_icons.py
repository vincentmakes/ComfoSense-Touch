#!/usr/bin/env python3
"""
Download Google Material Symbols weather icons and convert to LVGL L8 grayscale C arrays.

Uses the Material Symbols Outlined variant at 80x80px for e-paper display.
Icons are rendered black on white (inverted for e-paper readability).

Usage: python3 scripts/generate_weather_icons.py
"""

import os
import io
import requests
import cairosvg
from PIL import Image

DST_DIR = "src/t5/images"
ICON_SIZE = 80  # pixels

# Material Symbols icon names mapped to weather conditions
# Format: (ha_condition, icon_name, output_name)
WEATHER_ICONS = [
    ("sunny",           "wb_sunny",              "weather_sunny"),
    ("clear-night",     "nights_stay",            "weather_clear_night"),
    ("partlycloudy",    "partly_cloudy_day",      "weather_partly_cloudy"),
    ("cloudy",          "cloud",                  "weather_cloudy"),
    ("rainy",           "rainy",                  "weather_rainy"),
    ("pouring",         "thunderstorm",           "weather_pouring"),
    ("snowy",           "ac_unit",                "weather_snowy"),
    ("snowy-rainy",     "weather_mix",            "weather_sleet"),
    ("fog",             "foggy",                  "weather_fog"),
    ("windy",           "air",                    "weather_windy"),
    ("lightning",       "flash_on",               "weather_lightning"),
    ("lightning-rainy", "thunderstorm",           "weather_storm"),
    ("hail",            "grain",                  "weather_hail"),
    ("exceptional",     "warning",                "weather_exceptional"),
]

# Google Material Symbols SVG download URL
# Using the outlined variant, weight 400, optical size 48
def get_icon_url(icon_name):
    return (
        f"https://fonts.gstatic.com/s/i/short-term/release/"
        f"materialsymbolsoutlined/{icon_name}/default/48px.svg"
    )


def download_svg(icon_name):
    """Download SVG from Google Material Symbols."""
    url = get_icon_url(icon_name)
    print(f"  Downloading {url}")
    resp = requests.get(url, timeout=10)
    if resp.status_code != 200:
        print(f"  FAILED: HTTP {resp.status_code}")
        return None
    return resp.content


def svg_to_grayscale(svg_data, size):
    """Convert SVG to grayscale PIL Image at given size."""
    # Render SVG to PNG at target size
    png_data = cairosvg.svg2png(
        bytestring=svg_data,
        output_width=size,
        output_height=size,
    )

    # Open as PIL image and convert to grayscale
    img = Image.open(io.BytesIO(png_data)).convert("RGBA")

    # Composite onto white background (e-paper is white)
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    composite = Image.alpha_composite(bg, img)

    # Convert to grayscale
    gray = composite.convert("L")
    return gray


def generate_c_file(name, gray_image):
    """Generate an LVGL image C file in L8 format from a PIL grayscale image."""
    width, height = gray_image.size
    pixels = list(gray_image.getdata())
    upper_name = name.upper()

    lines = []
    lines.append('#include "lvgl.h"')
    lines.append("")
    lines.append(f"#ifndef LV_ATTRIBUTE_IMAGE_{upper_name}")
    lines.append(f"#define LV_ATTRIBUTE_IMAGE_{upper_name}")
    lines.append("#endif")
    lines.append("")
    lines.append(f"const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST "
                 f"LV_ATTRIBUTE_IMAGE_{upper_name} uint8_t {name}_map[] = {{")

    # Write pixel data in rows of 16 bytes
    for i in range(0, len(pixels), 16):
        chunk = pixels[i:i+16]
        hex_vals = ", ".join(f"0x{p:02x}" for p in chunk)
        comma = "," if i + 16 < len(pixels) else ""
        lines.append(f"  {hex_vals}{comma}")

    lines.append("};")
    lines.append("")
    lines.append(f"const lv_image_dsc_t {name} = {{")
    lines.append("  .header.cf = LV_COLOR_FORMAT_L8,")
    lines.append("  .header.magic = LV_IMAGE_HEADER_MAGIC,")
    lines.append(f"  .header.w = {width},")
    lines.append(f"  .header.h = {height},")
    lines.append(f"  .data_size = {len(pixels)},")
    lines.append(f"  .data = {name}_map,")
    lines.append("};")
    lines.append("")

    return "\n".join(lines)


def main():
    os.makedirs(DST_DIR, exist_ok=True)

    header_declares = []
    condition_map_entries = []
    converted = 0
    failed = []

    for ha_condition, icon_name, output_name in WEATHER_ICONS:
        print(f"Processing: {ha_condition} -> {icon_name} -> {output_name}")

        svg_data = download_svg(icon_name)
        if svg_data is None:
            failed.append((ha_condition, icon_name))
            continue

        gray_img = svg_to_grayscale(svg_data, ICON_SIZE)
        c_content = generate_c_file(output_name, gray_img)

        out_path = os.path.join(DST_DIR, f"{output_name}.c")
        with open(out_path, "w") as f:
            f.write(c_content)

        header_declares.append(f"LV_IMG_DECLARE({output_name});")
        condition_map_entries.append(f'    {{"{ha_condition}", &{output_name}}}')
        converted += 1
        print(f"  Written: {out_path} ({ICON_SIZE}x{ICON_SIZE})")

    # Generate header file with declarations and condition->icon mapping
    header_content = [
        "#ifndef T5_WEATHER_ICONS_H",
        "#define T5_WEATHER_ICONS_H",
        "",
        "#include <lvgl.h>",
        "",
    ]
    header_content.extend(header_declares)
    header_content.extend([
        "",
        "// Map HA weather condition string to icon image",
        "struct WeatherIconMap {",
        "    const char* condition;",
        "    const lv_image_dsc_t* icon;",
        "};",
        "",
        "static const WeatherIconMap weather_icon_map[] = {",
    ])
    header_content.extend([e + "," for e in condition_map_entries])
    header_content.extend([
        "};",
        "",
        f"static const int weather_icon_map_count = {len(condition_map_entries)};",
        "",
        "static inline const lv_image_dsc_t* get_weather_icon(const char* condition) {",
        "    for (int i = 0; i < weather_icon_map_count; i++) {",
        '        if (strcmp(weather_icon_map[i].condition, condition) == 0)',
        "            return weather_icon_map[i].icon;",
        "    }",
        "    return nullptr;",
        "}",
        "",
        "#endif // T5_WEATHER_ICONS_H",
        "",
    ])

    header_path = os.path.join(DST_DIR, "weather_icons.h")
    with open(header_path, "w") as f:
        f.write("\n".join(header_content))

    print(f"\nDone! Converted {converted} weather icons.")
    if failed:
        print(f"Failed ({len(failed)}): {[f'{c}/{i}' for c, i in failed]}")
    print(f"Header: {header_path}")


if __name__ == "__main__":
    main()
