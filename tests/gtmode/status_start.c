/* StatusRoot help "%Ba Transfer game data" versus authored Triangle
   branch gtmode.ad L14926..14934. Start is a PC hint-compatible alias. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
static unsigned dialogs;static char messages[8][512];
static int dialog(menu *m,const char *title,const char *text,int type,const char *yes,const char *no){(void)m;(void)title;(void)type;(void)yes;(void)no;CHECK(dialogs<8);snprintf(messages[dialogs++],512,"%s",text);return 0;}
int main(int argc,char **argv){
 CHECK(argc>=2);gt4_save_init("");gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol && gt4_game_load_disc(argv[1]));char error[256];menu *m=menu_open(vol,"gtmode",error,sizeof error);CHECK(m);menu_set_dialog_presenter(m,dialog);
 int root=-1;for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),"StatusRoot"))root=i;CHECK(root>=0);
 menu_enter(m,root);menu_tick(m);menu_send_key(m,0xff8d,0);CHECK(dialogs>0);unsigned expected=dialogs;char original[8][512];memcpy(original,messages,sizeof original);dialogs=0;
 menu_send_key(m,argc>2 && !strcmp(argv[2],"--negative")?0xff13+1:0xff13,0);
 CHECK(dialogs==expected);CHECK(!memcmp(messages,original,sizeof original));
 menu_send_key(m,0xff13,1);CHECK(dialogs==expected);
 menu_close(m);gt4_vol_close(vol);gt4_save_shutdown();puts("PASS Status Start alias equals authored import action/dialogs; release does not repeat");return 0;
}
