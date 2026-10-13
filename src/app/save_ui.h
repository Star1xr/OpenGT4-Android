#ifndef OPENGT4_SAVE_UI_H
#define OPENGT4_SAVE_UI_H
#include "menu.h"
typedef struct {menu *owner;ui_widget *page,*entry,*keyboard,*name_label,*cards[3],*pagination;ui_widget *welcome;int first_run,onboarding;int root,name_root,editing,typed,rename,slot,first,enter_gtmode;char name[97];} save_ui;
void save_ui_first_run(save_ui *s);
void save_ui_new_career(save_ui *s);
void save_ui_attach(save_ui *s,menu *m);
int save_ui_event(save_ui *s,int key,const char *text);
void save_ui_name(save_ui *s,const char *name);
#endif
