Import("env")
import os
import re

def fix_lvgl_compatibility(source, target, env):
    """Fix LVGL API compatibility issues between v8 and v9"""
    
    src_dir = "src/ui"
    
    replacements = [
        # Fix include path
        ('#include "lvgl/lvgl.h"', '#include "lvgl.h"'),
        # Fix type names (LVGL 9 -> LVGL 8)
        (r'\blv_screen_load_anim_t\b', 'lv_scr_load_anim_t'),
        (r'\blv_image_dsc_t\b', 'lv_img_dsc_t'),
        (r'\blv_image_header_t\b', 'lv_img_header_t'),
        (r'\blv_image_set_src\b', 'lv_img_set_src'),
        (r'\blv_image_create\b', 'lv_img_create'),
        (r'\blv_image_get_src\b', 'lv_img_get_src'),
        # Fix part names
        (r'\bLV_PART_LIST_MAIN\b', 'LV_PART_MAIN'),
        (r'\bLV_PART_CURSOR\b', 'LV_PART_CURSOR'),
        (r'\bLV_PART_INDICATOR\b', 'LV_PART_INDICATOR'),
        (r'\bLV_PART_KNOB\b', 'LV_PART_KNOB'),
        (r'\bLV_PART_SELECTED\b', 'LV_PART_SELECTED'),
        (r'\bLV_PART_ITEMS\b', 'LV_PART_ITEMS'),
        (r'\bLV_PART_TICKS\b', 'LV_PART_TICKS'),
    ]
    
    files_modified = []
    
    for root, dirs, files in os.walk(src_dir):
        for file in files:
            if file.endswith(('.c', '.h')):
                filepath = os.path.join(root, file)
                
                try:
                    with open(filepath, 'r', encoding='utf-8') as f:
                        content = f.read()
                    
                    original_content = content
                    
                    # Apply all replacements
                    for old, new in replacements:
                        content = re.sub(old, new, content)
                    
                    # Only write if content changed
                    if content != original_content:
                        with open(filepath, 'w', encoding='utf-8') as f:
                            f.write(content)
                        files_modified.append(filepath)
                        
                except Exception as e:
                    print(f"Error processing {filepath}: {e}")
    
    if files_modified:
        print(f"\n=== Fixed LVGL compatibility in {len(files_modified)} files ===")
        for f in files_modified:
            print(f"  - {f}")
        print()
    else:
        print("No LVGL compatibility fixes needed")

# Run immediately when script is loaded, before any compilation
print("=" * 60)
print("Running LVGL compatibility fixes...")
print("=" * 60)
fix_lvgl_compatibility(None, None, env)

# Also run before build to catch any changes
env.AddPreAction("buildprog", fix_lvgl_compatibility)