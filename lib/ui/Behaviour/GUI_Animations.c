#include "../GUI.h"



static void GUI_createTimelinePhase (lv_anim_timeline_t* timeline, uint32_t start_time, lv_anim_t* animation, lv_obj_t* object, lv_anim_custom_exec_cb_t animator, int32_t startvalue, int32_t endvalue, uint32_t duration, lv_anim_path_cb_t animpath, bool early_apply) {
    lv_anim_init( animation );
    lv_anim_set_var( animation, object );
    lv_anim_set_early_apply( animation, early_apply );
    lv_anim_set_custom_exec_cb( animation, animator );
    lv_anim_set_values( animation, startvalue, endvalue );
    lv_anim_set_duration( animation, duration );
    lv_anim_set_path_cb( animation, animpath );
    lv_anim_timeline_add( timeline, start_time, animation );
}


static void GUI_animatorCallback__Set_X (lv_anim_t * var, int32_t v) {
    lv_obj_set_x( var->var, v );
}


static void GUI_animatorCallback__Set_Y (lv_anim_t * var, int32_t v) {
    lv_obj_set_y( var->var, v );
}


static void GUI_animatorCallback__Set_WIDTH (lv_anim_t * var, int32_t v) {
    lv_obj_set_width( var->var, v );
}


static void GUI_animatorCallback__Set_HEIGHT (lv_anim_t * var, int32_t v) {
    lv_obj_set_height( var->var, v );
}


void GUI_initAnimations () {

}


