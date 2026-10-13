/*
 * Native UI: draws a GT4 UI screen (a RootWindow of a .mproject) with the
 * platform's 2D drawing. Replaces the PS2 path (widget code building GS
 * packets) with a direct interpretation of the same screen description.
 *
 * Each screen is loaded into a tree of widgets that the game's scripts can
 * change (visibility, position, images, copying items into lists); see
 * menu.h. Covered so far: layout, ColorFace, ImageFace, FrameImageFace,
 * visibility, opacity, containers, clipping, SelectBox showing its focused
 * item, FT01 text, RT03 translations and platform-provided MovieFace playback.
 * Widget rules that are assumptions are listed in docs/UI_FORMAT.md.
 */
#ifndef OPENGT4_UI_H
#define OPENGT4_UI_H
#include "tex1.h"
#include "ui_motion.h"
#include "car_service.h"
typedef struct plat_texture plat_texture;

#include "mproject.h"
#include "ui_motion.h"
typedef struct plat_texture plat_texture;
#include "vol.h"

typedef struct ui_project ui_project;
typedef struct ui_widget  ui_widget;

struct ui_widget {
    const mp_node *src;          /* description in the .mproject */
    const char    *type;         /* "ImageFace", "Composite", ... */
    const char    *name;
    ui_widget     *parent;
    ui_widget    **kids;
    int            nkids, kcap;
    int            root;         /* current owning screen (focus/actors) */
    int            resource_root; /* authored screen (packages/translations) */
    float          x, y, w, h;   /* relative to the parent */
    int            has_geometry;
    int            visible;
    int            insensitive;
    int            caret;        /* InputTextFace: the insertion point (bytes into text) */
    int            focused;
    int            active;       /* script `active`: state bit 1, inherited by children */
    uint8_t runtime_color[4],runtime_shadow[4];
    int has_runtime_color,has_runtime_shadow;
    int runtime_align,has_runtime_align;
    float          opacity;
    float          progress; /* ProgressFace, normalized fill */
    float          rotation; /* degrees around the widget centre */
    float          magnify;  /* actor magnification around the centre; 0 means 1 */
    float          model_angles[3],model_velocity[3];
    float          car_position_ratio; /* CarModel +58, posInterpolateSpeed */
    int            car_position_easing;
    int            model_target_mode; /* 0: velocity, 1: authored rest angles */
    char          *model_path;       /* setModelSetPath's model, over the authored model_set_path */
    char          *movie_name;
    int            movie_audio_track;
    double         flash_time;
    char          *flash_path;
    char          *image_path;   /* ImageFace */
    char          *text, *text_key; /* TextFace; owned by widget */
    char          *wrapped_text, *wrapped_source; /* layout cache; never alters script text */
    float          scale_x, scale_y, letter_spacing;
    ui_widget     *item_template;
    float          strip_offset;
    float          text_scroll;  /* ToolTipFace: seconds since the text changed */
    gt4_tooltip_motion tooltip_motion;
    int            strip_ready;
    float          scroll_tx, scroll_ty; /* scrolled "space": eased target position */
    int            scroll_set;
    /* MScaleBar 00239018/20: float value, clamped by script min/max. */
    float photo_value,photo_min,photo_max,photo_step,photo_current,photo_repeat;
    int            focus_index;  /* SelectBox */
    int slider_range_set,slider_min,slider_max,slider_display_set;
    double slider_display_min,slider_display_max;
    float (*graph_points)[2],graph_line_width;
    int graph_count,graph_segments,graph_type_set;
    int            option_open,option_pending; /* expanded choice, uncommitted */
    int            chip_kind;    /* ColorChipFace: 0 unset, 1 metallic, 2 solid */
    const struct gt4_mot1 *car_camera; /* borrowed project motion */
    float car_camera_time;int car_camera_loop,car_camera_playing;
    const uint8_t *car_crew_data;size_t car_crew_size; /* borrowed project model */
    uint64_t car_crew_serial; /* PC cache identity, survives allocator address reuse */
    const struct gt4_mot1 *car_crew_motion;
    float car_crew_time;int car_crew_playing,car_crew_loop;
    gt4_service_model *car_models;int car_model_count;
    gt4_service_motion *car_motions;int car_motion_count;
    double car_service_ticks;
    float car_model_opacity;
    float car_target_position[3],car_target_angles[3];
    float car_service_geometry[12],car_additional_scale; /* alignment[8], suspension drop[4] */
    int car_service_geometry_valid;
    char          *car_wing;
    char          *car_wheel;    /* CarFace.wheel try-on label from original shop script */
    char          *car_code;     /* CarFace: the script's car_code, NULL before */
    int            car_color_index; /* paint belongs to this CarFace, -1 before selection */
    int            car_pose_set; /* script supplied a CarModel pose */
    int            car_motion; /* CarModel.motion: 0 manual, 1 turntable, 2 scripted motion */
    float          car_spin;
    double         car_spin_ticks;
    float          car_position[3];
    int            car_normal;   /* CarFace: action_type "normal" (no studio) */
    uint8_t        chip_rgb[3];
    uint8_t        chip_bands[4][3]; /* ColorChipFace.setColor(colours): the quick menus' bands */
    int            chip_band_count;
    unsigned       drawn_tick;   /* ui_draw_tick() when last drawn */
    void          *script;       /* the script object bound to this widget, if any */
};

/* Loads /projects/<name>/<name>.mproject and its packages from the volume. */
ui_project *ui_project_load(gt4_vol *vol, const char *name, char *err, size_t err_len);
void        ui_project_free(ui_project *p);
const mp_node *ui_project_tree(const ui_project *p);

int         ui_root_count(const ui_project *p);
const char *ui_root_name(const ui_project *p, int i);
ui_widget  *ui_root_widget(ui_project *p, int i);
void ui_set_prepare_presenter(ui_project *p,void (*fn)(void *),void *user);
/* CarFace's draw-side camera-controller normalization, 00156F18. */
void ui_car_prepare_draw(ui_widget *w);
/* LightSet constructor defaults in authored colour units (255 = 1). */
void ui_light_defaults(double world[4],double position[4],double ambient[4],double diffuse[4],double specular[4]);
void ui_prepare_dialogs(ui_project *p,void (*progress)(void *),void *user);
void ui_prepare_root(ui_project *p,int root,void (*progress)(void *),void *user);

/* Deep copy of a widget (detached, same screen); owned by the project. */
/* Parse a native widget description; project owns its source and runtime. */
ui_widget *ui_widget_from_text(ui_project *p,int resource_root,const char *description);
ui_widget *ui_widget_copy(ui_project *p, const ui_widget *w);
void       ui_widget_append(ui_widget *parent, ui_widget *child);
void       ui_widget_detach(ui_widget *w);
ui_widget *ui_widget_child(const ui_widget *w, const char *name);
void       ui_widget_set_image(ui_widget *w, const char *path);
/* an image made at run time (a saved replay's picture), found by path as
   the package's are; replaces one of the same path */
void ui_image_draw(ui_project *p,const char *path,float x,float y,float w,float h,float alpha);
void       ui_image_put(ui_project *p, const char *path, int w, int h, const uint8_t *rgba);
const char *ui_widget_text(ui_project *p, const ui_widget *w);
const char *ui_widget_source_text(ui_project *p,const ui_widget *w);
/* P: device references in authored UI become native local-storage labels. */
const char *ui_storage_text(const char *text);
const char *ui_translate(ui_project *p, int root, const char *page, const char *key);
int ui_option_count(const ui_widget *w);
ui_widget *ui_option_item(const ui_widget *w,int index);
struct gt4_mot1 *ui_load_camera(ui_project *p,int root,const char *path);
struct gt4_mot1 *ui_load_motion(ui_project *p,int root,const char *path);
const uint8_t *ui_load_model_bytes(ui_project *p,int root,const char *path,size_t *size);
int ui_load_package(ui_project *p,const char *directory,const char *name);
void ui_adjust_size(ui_project *p,ui_widget *w);
void ui_text_adjust_scale(ui_project *p, ui_widget *w);
int ui_text_abbreviate(ui_project *p,ui_widget *w);
void ui_text_wrap(ui_project *p,ui_widget *w,float width);
void ui_text_size(ui_project *p,const ui_widget *w,float size[2]);
/* Sets the text of every ToolTipFace under w (NULL clears) and restarts its scrolling. */
void ui_set_tooltips(ui_widget *w, const char *text);
void ui_reset_tooltips(ui_widget *w);
void ui_cursor_speed(ui_project *p,float ratio);
/* The context focus owns the cursor; decorative lists cannot replace it. */
void ui_cursor_select(ui_project *p,const ui_widget *widget);
int ui_cursor_pause(ui_project *p,int paused);
/* Retain position and target while a selected modal panel exits. */
void ui_cursor_hold(ui_project *p,int held);
/* Per render-context visibility; position persists while hidden. */
void ui_cursor_show(ui_project *p,int visible);
void ui_cursor_icon(ui_project *p,const char *name);
/* Common dialogs use the same original render context cursor. */
void ui_cursor_transfer(ui_project *destination,const ui_project *source);
int ui_package_contains(ui_project *p,int root,const char *path);

typedef int (*ui_movie_draw_fn)(void *user,const char *name,float x,float y,float w,float h,float opacity,int interlace,int repeat,int audio_track);
void ui_set_movie_renderer(ui_project *p,ui_movie_draw_fn draw,void *user);

/* Platform-owned meshes, cached for the lifetime of the UI project. */
typedef struct {
    void *(*create)(const uint8_t *data,size_t size);
    void (*draw)(void *mesh,const mp_node *description,const float angles[3],float x,float y,float w,float h,const tex1_image *environment);
    void (*destroy)(void *mesh);
} ui_model_renderer;
void ui_set_model_renderer(ui_project *p,ui_model_renderer renderer);
/* A CarFace with a car is drawn in place (after the widgets before it) by
   this platform hook: the widget (car_code, car_normal, its description's
   camera and lights) and its frame in the 640x480 UI space. */
typedef void (*ui_car_draw_fn)(void *user,const ui_widget *w,float x,float y,float wd,float h,float opacity);
void ui_set_car_renderer(ui_project *p,ui_car_draw_fn draw,void *user);
typedef int (*ui_scene_draw_fn)(void *user,float x,float y,float w,float h,float opacity);
void ui_set_scene_renderer(ui_project *p,ui_scene_draw_fn draw,void *user);
typedef int (*ui_photo_draw_fn)(void *user,const ui_widget *widget,float x,float y,float w,float h,float opacity);
void ui_set_album_renderer(ui_project *p,ui_photo_draw_fn draw,void *user);
void ui_set_photo_renderer(ui_project *p,ui_photo_draw_fn draw,void *user);
/* LoggerFace and RaceCourseMapFace: drawn by their owner */
void ui_set_native_renderer(ui_project *p,ui_photo_draw_fn draw,void *user);
/* plain text in a disc face: (x, y) top left, or ending at x + right when
   right > 0; returns its width */
float ui_draw_string(ui_project *p,const char *face,float x,float y,float scale,float right,const uint8_t rgba[4],const char *text);
float ui_string_height(ui_project *p,const char *face,float scale);
/* Presentation fade covers 2D and ModelFace content alike. */
void ui_fade(ui_project *p,float from,float to,float seconds);
float ui_fade_remaining(const ui_project *p);
/* Retained render-context surface; capture occurs at the platform boundary. */
void ui_set_capture_renderer(ui_project *p,plat_texture *(*capture)(plat_texture *,float,float,float,float));
/* the screen's region (x, y, w, h of 640x480) into t (made or reused); a
   captured texture runs v from 1 at its top to 0 */
plat_texture *ui_capture_region(ui_project *p,plat_texture *t,float x,float y,float w,float h);
gt4_transition_motion *ui_transition_state(ui_project *p);
void ui_transition_start(ui_project *p,int kind,int operation,float period);
void ui_transition_reveal(ui_project *p);
float ui_transition_remaining(const ui_project *p);


/* Draws root window i in the 640x480 UI space. Returns counts of widgets
   drawn and skipped (types not implemented yet) for diagnostics. */
void ui_draw_caption(ui_project *p,int root,const char *text);
void ui_animate(ui_project *p,int root,float seconds);
void ui_draw_root(ui_project *p, int i, int *drawn, int *skipped);
/* Counts ui_draw_root calls; 0 before the first. A widget whose drawn_tick
   equals it was drawn (visible, state conditions met) by the last call. */
unsigned ui_draw_tick(const ui_project *p);
/* Draws a page lower in the stack (under a pushed one) before the frame's
   ui_draw_root: no cursor, no fade, counted as drawn by that same call. */
void ui_draw_under(ui_project *p, int i);
/* Directory of the local HD image cache (tools/c/gt4hdui.c); "" or NULL
   keeps the disc's images. Process-wide; set before pages load. */
void ui_set_hd_cache(const char *dir);

#endif /* OPENGT4_UI_H */
