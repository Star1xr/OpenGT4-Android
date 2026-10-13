#ifndef OPENGT4_PHOTO_TRAVEL_H
#define OPENGT4_PHOTO_TRAVEL_H
#include "menu.h"
typedef struct photo_travel photo_travel;
photo_travel *photo_travel_open(const char *disc);
void photo_travel_close(photo_travel *p);
/* P: PC keyboard shortcut and export notification at the storage boundary. */
void photo_travel_shoot(photo_travel *p);
void photo_travel_feedback(photo_travel *p);
const char *photo_travel_last_saved(const photo_travel *p);
/* Resolve fitted parts in the source selection, before changing the cached editor. */
int photo_travel_load_from(photo_travel *p,menu *destination,menu *source,const char *course,const char *car,int color,void (*pump)(void *),void *user);
int photo_travel_load(photo_travel *p,menu *m,const char *course,const char *car,int color,void (*pump)(void *),void *user);
struct car_model_tag;
const struct car_model_tag *photo_travel_car_model(const photo_travel *p);
int photo_travel_draw(void *user,const ui_widget *widget,float x,float y,float w,float h,float opacity);
struct race_view;
int photo_travel_drive(photo_travel *,menu *,struct race_view *);
void photo_travel_drive_setup(photo_travel *,const float size[4],float distance);
void photo_travel_drive_camera(photo_travel *,int delta);
void photo_travel_drive_end(photo_travel *);
int photo_travel_drive_preview(photo_travel *,menu *preview);
int photo_travel_develop(photo_travel *,menu *,const char *film,void (*pump)(void *),void *user);
void photo_travel_drive_editor(photo_travel *);
void photo_travel_editor_draw(photo_travel *);
#endif
