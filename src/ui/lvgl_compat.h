#ifndef LVGL_COMPAT_H
#define LVGL_COMPAT_H

#include "lvgl.h"

// LVGL 8.x compatibility macros
#if LV_VERSION_CHECK(8, 0, 0)
    #define lv_scr_load_anim_t lv_scr_load_anim_t
    #define LV_PART_MAIN LV_PART_MAIN
#endif

#endif // LVGL_COMPAT_H