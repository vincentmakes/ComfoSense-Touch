#include "GUI.h"



void GUI_load () {
    GUI_initFramework();
    GUI_initHAL();
    GUI_loadContent();
}


void GUI_init () {
    GUI_loadContent();
}


void GUI_refresh () {
    lv_timer_handler();
    //...
}


void GUI_initHAL () {
    //...
}


void GUI_initFramework () {
    lv_init();
}


void GUI_loadContent () {
    GUI_initContent();
    GUI_loadFirstScreen();
}


void GUI_initContent () {
    GUI_initTheme();
    GUI_initGlobalStyles();
    GUI_initScreens();
    GUI_initAnimations();
}


void GUI_initTheme () {
    lv_display_t* Display = lv_display_get_default();
    lv_theme_t* Theme = lv_theme_default_init( Display, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, lv_font_default() );
    lv_display_set_theme( Display, Theme );
}


void GUI_initScreens () {
    GUI_initScreenContents();
}


void GUI_loadFirstScreen () {
    lv_screen_load( GUI_Screen__screen );
}


void GUI_initScreenContents () {
    GUI_initScreen__screen();
}


void GUI_initScreenTexts () {
    GUI_initScreenTexts__screen();
}
void GUI_initScreenStyles () {
    GUI_initScreenStyles__screen();
}
