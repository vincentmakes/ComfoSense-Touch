#!/usr/bin/env python3
"""
Convert LVGL ARGB8888 image C arrays to L8 (8-bit grayscale) for e-paper display.

Reads the existing color image C files and generates grayscale versions
in src/t5/images/ for use with the T5 e-paper display.

Usage: python3 scripts/convert_images_to_grayscale.py
"""

import re
import os
import sys

# Source and destination directories
SRC_DIR = "src/ui/Content/assets/images"
DST_DIR = "src/t5/images"

# Images to convert
IMAGES = [
    ("upload_fan0_4bd7cbea47ab4747a7ba1a8e654304d3_png", "fan0_gray"),
    ("upload_fan1_5f7d42737550431ebd8adb45b281d499_png", "fan1_gray"),
    ("upload_fan2_4980bc102f3943d19cdcd71981ffdd64_png", "fan2_gray"),
    ("upload_fan3_84f063b5abbd4a2eb99f1df624dad654_png", "fan3_gray"),
    ("upload_fanboost_09b958c807d44065830a9fbfb5e59bb8_png", "fanboost_gray"),
    ("upload_minus_22b4150ed8454f4f8d0531614cfc727c_png", "minus_gray"),
    ("upload_plus_68d2c2cb30b349e2b7f71cb50dfbe8b8_png", "plus_gray"),
    ("upload_warningoutline_2343dccdd2da484cab2db0b1a6f4ab6b_png", "warning_gray"),
    ("upload_wifioutline_1922d60a08324fb58b6286008444cc13_png", "wifi_gray"),
]


def extract_image_data(filepath):
    """Extract hex bytes, width, height from an LVGL image C file."""
    with open(filepath, "r") as f:
        content = f.read()

    # Extract dimensions
    w_match = re.search(r'\.header\.w\s*=\s*(\d+)', content)
    h_match = re.search(r'\.header\.h\s*=\s*(\d+)', content)
    if not w_match or not h_match:
        print(f"  ERROR: Could not find dimensions in {filepath}")
        return None, 0, 0

    width = int(w_match.group(1))
    height = int(h_match.group(1))

    # Extract hex byte array
    # Match the array content between { and };
    array_match = re.search(r'uint8_t\s+\w+\[\]\s*=\s*\{(.*?)\};', content, re.DOTALL)
    if not array_match:
        print(f"  ERROR: Could not find byte array in {filepath}")
        return None, 0, 0

    hex_str = array_match.group(1)
    # Parse hex values
    hex_values = re.findall(r'0x([0-9a-fA-F]{2})', hex_str)
    raw_bytes = bytes(int(h, 16) for h in hex_values)

    return raw_bytes, width, height


def argb8888_to_l8(raw_bytes, width, height):
    """Convert ARGB8888 pixel data to L8 grayscale.

    LVGL ARGB8888 byte order: B, G, R, A (little-endian)
    Alpha-blend against white background (255) for e-paper.
    Output: single byte per pixel (0=black, 255=white).
    """
    num_pixels = width * height
    expected_bytes = num_pixels * 4

    if len(raw_bytes) < expected_bytes:
        print(f"  WARNING: Expected {expected_bytes} bytes, got {len(raw_bytes)}")
        # Pad with zeros
        raw_bytes = raw_bytes + b'\x00' * (expected_bytes - len(raw_bytes))

    grayscale = bytearray(num_pixels)

    for i in range(num_pixels):
        offset = i * 4
        b = raw_bytes[offset]
        g = raw_bytes[offset + 1]
        r = raw_bytes[offset + 2]
        a = raw_bytes[offset + 3]

        # Luminance formula
        luma = int(0.299 * r + 0.587 * g + 0.114 * b)

        # Alpha-blend against white background (e-paper is white)
        alpha_f = a / 255.0
        blended = int(luma * alpha_f + 255 * (1.0 - alpha_f))

        grayscale[i] = min(255, max(0, blended))

    return bytes(grayscale)


def generate_c_file(name, gray_data, width, height):
    """Generate an LVGL image C file in L8 format."""
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
    for i in range(0, len(gray_data), 16):
        chunk = gray_data[i:i+16]
        hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
        comma = "," if i + 16 < len(gray_data) else ""
        lines.append(f"  {hex_vals}{comma}")

    lines.append("};")
    lines.append("")
    lines.append(f"const lv_image_dsc_t {name} = {{")
    lines.append("  .header.cf = LV_COLOR_FORMAT_L8,")
    lines.append("  .header.magic = LV_IMAGE_HEADER_MAGIC,")
    lines.append(f"  .header.w = {width},")
    lines.append(f"  .header.h = {height},")
    lines.append(f"  .data_size = {len(gray_data)},")
    lines.append(f"  .data = {name}_map,")
    lines.append("};")
    lines.append("")

    return "\n".join(lines)


def main():
    os.makedirs(DST_DIR, exist_ok=True)

    # Generate header with all image declarations
    header_lines = [
        "#ifndef T5_IMAGES_H",
        "#define T5_IMAGES_H",
        "",
        "#include <lvgl.h>",
        "",
    ]

    converted = 0
    for src_name, dst_name in IMAGES:
        src_path = os.path.join(SRC_DIR, f"{src_name}.c")
        dst_path = os.path.join(DST_DIR, f"{dst_name}.c")

        if not os.path.exists(src_path):
            print(f"SKIP: {src_path} not found")
            continue

        print(f"Converting {src_name} -> {dst_name}...")

        raw_bytes, width, height = extract_image_data(src_path)
        if raw_bytes is None:
            continue

        print(f"  Size: {width}x{height}, {len(raw_bytes)} bytes ARGB8888")

        gray_data = argb8888_to_l8(raw_bytes, width, height)
        print(f"  Grayscale: {len(gray_data)} bytes L8 "
              f"(saved {len(raw_bytes) - len(gray_data)} bytes)")

        c_content = generate_c_file(dst_name, gray_data, width, height)

        with open(dst_path, "w") as f:
            f.write(c_content)

        header_lines.append(f"LV_IMG_DECLARE({dst_name});")
        converted += 1
        print(f"  Written: {dst_path}")

    header_lines.extend(["", "#endif // T5_IMAGES_H", ""])

    header_path = os.path.join(DST_DIR, "t5_images.h")
    with open(header_path, "w") as f:
        f.write("\n".join(header_lines))

    print(f"\nDone! Converted {converted} images.")
    print(f"Header: {header_path}")


if __name__ == "__main__":
    main()
