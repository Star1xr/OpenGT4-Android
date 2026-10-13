#ifndef OPENGT4_VIDEO_SETTINGS_H
#define OPENGT4_VIDEO_SETTINGS_H
#include "platform/platform.h"
#include "ui.h"
#include "menu.h"

typedef struct {
    plat_video_desc video;
    int filter, text, aspect;
} pc_video_options;
typedef struct {
    pc_video_options current,draft;
    int persist,root,option_root;
    menu *owner;
    ui_widget *page,*choices[7];
    char path[1024],message[128];
} video_settings;
void video_settings_init(video_settings *s,int persist,const char *config_file);
void video_settings_attach(video_settings *s,menu *m);
#endif
