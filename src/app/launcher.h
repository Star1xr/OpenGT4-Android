#ifndef OPENGT4_LAUNCHER_H
#define OPENGT4_LAUNCHER_H
#include "pad_config.h"
#include <stddef.h>
#define LAUNCHER_PATH 1024
/* P: PC preparation independent of every disc asset. Layout/system fonts are
   PC choices, not reconstructed PS2 artwork. No simulation changes. */
typedef struct {
    char disc[LAUNCHER_PATH],saves[LAUNCHER_PATH],file[LAUNCHER_PATH];
    int show;
} launcher_preferences;
typedef struct {
    int width,height,fullscreen,vsync,upscale,filter,text,aspect;
    int volume[7];
    gt4_pad_config pad[2];
} launcher_options;
typedef struct {
    launcher_preferences preferences;
    char video_file[LAUNCHER_PATH];
    char *arguments[7];
} launcher_startup;
extern const char *const launcher_volume_keys[7];
void launcher_options_defaults(launcher_options *o);
int launcher_options_load(launcher_options *o,const char *video,const char *saves);
/* Groups: 1 video, 2 audio, 4 pad 1P, 8 pad 2P. Preserve other options and
   merge only edited options into active careers through native save APIs. */
int launcher_options_save(const launcher_options *o,const char *video,const char *saves,int groups);
int launcher_preferences_load(launcher_preferences *p,const char *file);
int launcher_preferences_save(const launcher_preferences *p);
int launcher_validate_disc(const char *path,char *message,size_t size);
/* No args / --launcher enters preparation; explicit game CLI stays intact.
   Returns 1 to run the game, 0 on cancel, -1 on failure. */
int launcher_capture_gallery(const char *disc,const char *folder);
int launcher_bootstrap(int *argc,char ***argv,launcher_startup *startup);
#endif
