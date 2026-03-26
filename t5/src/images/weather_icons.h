#ifndef T5_WEATHER_ICONS_H
#define T5_WEATHER_ICONS_H

#include <lvgl.h>

LV_IMG_DECLARE(weather_sunny);
LV_IMG_DECLARE(weather_clear_night);
LV_IMG_DECLARE(weather_partly_cloudy);
LV_IMG_DECLARE(weather_cloudy);
LV_IMG_DECLARE(weather_rainy);
LV_IMG_DECLARE(weather_pouring);
LV_IMG_DECLARE(weather_snowy);
LV_IMG_DECLARE(weather_sleet);
LV_IMG_DECLARE(weather_fog);
LV_IMG_DECLARE(weather_windy);
LV_IMG_DECLARE(weather_lightning);
LV_IMG_DECLARE(weather_storm);
LV_IMG_DECLARE(weather_hail);
LV_IMG_DECLARE(weather_exceptional);

// Map HA weather condition string to icon image
struct WeatherIconMap {
    const char* condition;
    const lv_image_dsc_t* icon;
};

static const WeatherIconMap weather_icon_map[] = {
    {"sunny", &weather_sunny},
    {"clear-night", &weather_clear_night},
    {"partlycloudy", &weather_partly_cloudy},
    {"cloudy", &weather_cloudy},
    {"rainy", &weather_rainy},
    {"pouring", &weather_pouring},
    {"snowy", &weather_snowy},
    {"snowy-rainy", &weather_sleet},
    {"fog", &weather_fog},
    {"windy", &weather_windy},
    {"lightning", &weather_lightning},
    {"lightning-rainy", &weather_storm},
    {"hail", &weather_hail},
    {"exceptional", &weather_exceptional},
};

static const int weather_icon_map_count = 14;

static inline const lv_image_dsc_t* get_weather_icon(const char* condition) {
    for (int i = 0; i < weather_icon_map_count; i++) {
        if (strcmp(weather_icon_map[i].condition, condition) == 0)
            return weather_icon_map[i].icon;
    }
    return nullptr;
}

#endif // T5_WEATHER_ICONS_H
