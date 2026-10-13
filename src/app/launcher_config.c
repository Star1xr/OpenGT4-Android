#include "launcher.h"
#include "platform/platform.h"
#include "vol.h"
#include "save.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static FILE *open_file(const char *path,const char *mode)
{
#ifdef _WIN32
    wchar_t wide[LAUNCHER_PATH],wm[16];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,LAUNCHER_PATH) ||
       !MultiByteToWideChar(CP_UTF8,0,mode,-1,wm,16))return NULL;
    return _wfopen(wide,wm);
#else
    return fopen(path,mode);
#endif
}
static int copy_path(char *out,const char *value)
{
    if(!value || strlen(value)>=LAUNCHER_PATH || strpbrk(value,"\r\n"))return 0;
    strcpy(out,value);return 1;
}
int launcher_preferences_load(launcher_preferences *p,const char *file)
{
    memset(p,0,sizeof *p);p->show=1;if(!copy_path(p->file,file))return 0;
    FILE *f=open_file(file,"rb");if(!f)return 1;
    char line[LAUNCHER_PATH+32];int ok=1;
    while(fgets(line,sizeof line,f)){
        size_t n=strlen(line);if(n && line[n-1]!='\n' && !feof(f)){ok=0;break;}
        line[strcspn(line,"\r\n")]=0;
        if(!strncmp(line,"disc=",5))ok=copy_path(p->disc,line+5);
        else if(!strncmp(line,"saves=",6))ok=copy_path(p->saves,line+6);
        else if(!strcmp(line,"show=0"))p->show=0;
        else if(!strcmp(line,"show=1"))p->show=1;
        if(!ok)break;
    }
    if(ferror(f))ok=0;fclose(f);
    if(!ok){p->disc[0]=p->saves[0]=0;p->show=1;}return ok;
}
int launcher_preferences_save(const launcher_preferences *p)
{
    if(!p->file[0] || strpbrk(p->disc,"\r\n") || strpbrk(p->saves,"\r\n"))return 0;
    char tmp[LAUNCHER_PATH+8];snprintf(tmp,sizeof tmp,"%s.tmp",p->file);
    FILE *f=open_file(tmp,"wb");if(!f)return 0;
    int ok=fprintf(f,"disc=%s\nsaves=%s\nshow=%d\n",p->disc,p->saves,p->show!=0)>0;
    if(fclose(f))ok=0;
#ifdef _WIN32
    wchar_t a[LAUNCHER_PATH+8],b[LAUNCHER_PATH];
    if(!MultiByteToWideChar(CP_UTF8,0,tmp,-1,a,LAUNCHER_PATH+8) || !MultiByteToWideChar(CP_UTF8,0,p->file,-1,b,LAUNCHER_PATH))return 0;
    if(ok)ok=MoveFileExW(a,b,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(a);
#else
    if(ok)ok=rename(tmp,p->file)==0;if(!ok)remove(tmp);
#endif
    return ok;
}
int launcher_validate_disc(const char *path,char *message,size_t size)
{
    const char *why=NULL;size_t n=0;int ok=0;uint8_t *cnf=NULL,*elf=NULL,*core=NULL;
    plat_file *file=path && *path?plat_file_open(path):NULL;
    if(!file){snprintf(message,size,"The disc image could not be found. Please locate it again.");return 0;}
    plat_file_close(file);
    cnf=gt4_disc_read_root(path,"SYSTEM.CNF",&n);
    if(!cnf || !n || !memchr(cnf,'\n',n) || memchr(cnf,0,n)){
        snprintf(message,size,"This file is not a PS2 disc image, or it could not be read.");goto done;}
    /* P: any SCUS-97328 release is accepted (user decision 12/10/2026):
       the disc is identified by its boot file in SYSTEM.CNF, and its
       version is shown. The game's files must be present and the volume
       must open. Fingerprints of v2.00 are in config/supported_discs.json;
       no executable/asset bytes are distributed. */
    {
        char *text=malloc(n+1);if(!text)goto done;memcpy(text,cnf,n);text[n]=0;
        char version[16]="";const char *v=strstr(text,"VER");
        if(v){v+=3;while(*v==' ' || *v=='=' || *v=='\t')v++;
            size_t k=0;while(k<sizeof version-1 && v[k] && v[k]!='\r' && v[k]!='\n' && v[k]!=' ')version[k]=v[k],k++;version[k]=0;}
        int game=strstr(text,"SCUS_973.28")!=NULL;free(text);
        if(!game){snprintf(message,size,"This is not the NTSC-U Gran Turismo 4 disc (SCUS-97328).");goto done;}
        elf=gt4_disc_read_root(path,"SCUS_973.28",&n);
        core=gt4_disc_read_root(path,"CORE.GT4",&n);
        gt4_vol *vol=elf && core?gt4_vol_open(path,&why):NULL;
        if(!vol){snprintf(message,size,"SCUS-97328, but the game's files could not be read: the image looks damaged.");goto done;}
        gt4_vol_close(vol);
        snprintf(message,size,"Gran Turismo 4 - NTSC-U / SCUS-97328 / v%s",*version?version:"?");ok=1;
    }
done:
    free(cnf);free(elf);free(core);return ok;
}

const char *const launcher_volume_keys[7]={"menu_bgm_volume_raw","menu_se_volume_raw",
    "race_bgm_volume_raw","race_se_volume_raw","replay_bgm_volume_raw","replay_se_volume_raw","slide_bgm_volume_raw"};
static const char *const pad_keys[4]={"pad_buttons_1p","pad_analog_1p","pad_buttons_2p","pad_analog_2p"};
void launcher_options_defaults(launcher_options *o)
{
    memset(o,0,sizeof *o);o->width=1280;o->height=960;o->vsync=1;
    /* Existing PC video defaults; original option.ad/config-main.ad volume
       raw defaults 120 and pad_config.c DS2 defaults (004382B0/F8). */
    for(int i=0;i<7;i++)o->volume[i]=120;
    for(int p=0;p<2;p++)gt4_pad_defaults(&o->pad[p],p);
}
static int *pad_values(launcher_options *o,int k){return k%2?o->pad[k/2].analog:o->pad[k/2].button;}
static int numbers(const char *text,int *out,int count,int low,int high)
{
    int temp[16];const char *at=text;char *end;
    for(int i=0;i<count;i++){
        long n=strtol(at,&end,10);if(at==end || n<low || n>high)return 0;
        temp[i]=(int)n;at=end;
        if(i<count-1){if(*at!=',')return 0;at++;}
    }
    if(*at)return 0;memcpy(out,temp,count*sizeof *out);return 1;
}
int launcher_options_load(launcher_options *o,const char *video,const char *saves)
{
    launcher_options_defaults(o);FILE *f=open_file(video,"rb");char line[1024],key[64],payload[768];int n,type;
    if(f){while(fgets(line,sizeof line,f))if(sscanf(line,"%63[^=]=%d",key,&n)==2){
        if(!strcmp(key,"width") && n>=640 && n<=7680)o->width=n;
        else if(!strcmp(key,"height") && n>=480 && n<=4320)o->height=n;
        else if(!strcmp(key,"fullscreen") && n>=0 && n<=1)o->fullscreen=n;
        else if(!strcmp(key,"vsync") && n>=0 && n<=1)o->vsync=n;
        else if(!strcmp(key,"upscaling") && n>=0 && n<=4)o->upscale=n;
        else if(!strcmp(key,"ui_filter") && n>=0 && n<=2)o->filter=n;
        else if(!strcmp(key,"text_sharpness") && n>=0 && n<=100)o->text=n;
        else if(!strcmp(key,"aspect_ratio") && n>=0 && n<=1)o->aspect=n;
    }fclose(f);}
    char path[LAUNCHER_PATH+16];snprintf(path,sizeof path,"%s.game_options",video);f=open_file(path,"rb");
    if(f){while(fgets(line,sizeof line,f)){
        line[strcspn(line,"\r\n")]=0;
        if(sscanf(line,"%63s %d %767[^\n]",key,&type,payload)!=3)continue;
        for(int i=0;i<7;i++)if(!strcmp(key,launcher_volume_keys[i]) && type==0)numbers(payload,&o->volume[i],1,0,128);
        for(int i=0;i<4;i++)if(!strcmp(key,pad_keys[i]) && type==2)numbers(payload,pad_values(o,i),i%2?6:16,i%2?-2:-1,GT4_PAD_ACTIONS-1);
    }fclose(f);}
    /* Career options have precedence in the existing game. Read them without
       loading a disc/VM script; Apply merges only the requested fields. */
    gt4_save_init(saves);
    if(gt4_save_active()){
        for(int i=0;i<7;i++){double v=gt4_save_option_number(launcher_volume_keys[i],o->volume[i]);if(v>=0 && v<=128)o->volume[i]=(int)v;}
        for(int i=0;i<4;i++)gt4_save_option_array_get(pad_keys[i],pad_values(o,i),i%2?6:16);
    }
    gt4_save_shutdown();return 1;
}
static int replace_file(const char *tmp,const char *path)
{
#ifdef _WIN32
    wchar_t a[LAUNCHER_PATH+32],b[LAUNCHER_PATH+32];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,tmp,-1,a,LAUNCHER_PATH+32) ||
       !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,b,LAUNCHER_PATH+32))return 0;
    int ok=MoveFileExW(a,b,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(a);return ok;
#else
    int ok=rename(tmp,path)==0;if(!ok)remove(tmp);return ok;
#endif
}
static int edited_key(const char *key,int groups)
{
    for(int i=0;i<7;i++)if(groups&2 && !strcmp(key,launcher_volume_keys[i]))return 1;
    for(int i=0;i<4;i++)if(groups&(4<<(i/2)) && !strcmp(key,pad_keys[i]))return 1;
    return 0;
}
int launcher_options_save(const launcher_options *o,const char *video,const char *saves,int groups)
{
    if(!video || strlen(video)>=LAUNCHER_PATH || !groups || groups&~15)return 0;
    if(o->width<640 || o->width>7680 || o->height<480 || o->height>4320 ||
       o->fullscreen<0 || o->fullscreen>1 || o->vsync<0 || o->vsync>1 ||
       o->upscale<0 || o->upscale>4 || o->filter<0 || o->filter>2 ||
       o->text<0 || o->text>100 || o->aspect<0 || o->aspect>1)return 0;
    for(int i=0;i<7;i++)if(o->volume[i]<0 || o->volume[i]>128)return 0;
    for(int p=0;p<2;p++){
        for(int i=0;i<16;i++)if(o->pad[p].button[i]<-1 || o->pad[p].button[i]>=GT4_PAD_ACTIONS)return 0;
        for(int i=0;i<6;i++)if(o->pad[p].analog[i]<-2 || o->pad[p].analog[i]>=GT4_PAD_ACTIONS)return 0;
    }
    char path[LAUNCHER_PATH+16],tmp[LAUNCHER_PATH+32];int ok=1;
    if(groups&1){
        snprintf(tmp,sizeof tmp,"%s.tmp",video);FILE *f=open_file(tmp,"wb");if(!f)return 0;
        ok=fprintf(f,"width=%d\nheight=%d\nfullscreen=%d\nvsync=%d\nupscaling=%d\nui_filter=%d\ntext_sharpness=%d\naspect_ratio=%d\n",
            o->width,o->height,o->fullscreen,o->vsync,o->upscale,o->filter,o->text,o->aspect)>0;
        if(fclose(f))ok=0;if(!ok || !replace_file(tmp,video))return 0;
    }
    if(!(groups&14))return 1;
    snprintf(path,sizeof path,"%s.game_options",video);snprintf(tmp,sizeof tmp,"%s.tmp",path);
    FILE *source=open_file(path,"rb"),*dest=open_file(tmp,"wb");if(!dest){if(source)fclose(source);return 0;}
    char line[1024],key[64];
    if(source){while(fgets(line,sizeof line,source)){
        if(!strchr(line,'\n') && !feof(source)){ok=0;break;}
        if(sscanf(line,"%63s",key)==1 && edited_key(key,groups))continue;
        if(fputs(line,dest)==EOF)ok=0;
        if(*line && line[strlen(line)-1]!='\n' && fputc('\n',dest)==EOF)ok=0;
    }if(ferror(source))ok=0;fclose(source);}
    ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);
    ah_set_member(vm,ah_root(vm),"game",ah_objv(game));ah_obj *option=ah_module_new(vm,"option",game,NULL,NULL);
    ah_set_member(vm,game,"option",ah_objv(option));
    if(groups&2)for(int i=0;i<7;i++){
        if(fprintf(dest,"%s 0 %d\n",launcher_volume_keys[i],o->volume[i])<0)ok=0;
        ah_set_member(vm,option,launcher_volume_keys[i],ah_int(o->volume[i]));
    }
    for(int i=0;i<4;i++)if(groups&(4<<(i/2))){
        int count=i%2?6:16;const int *values=i%2?o->pad[i/2].analog:o->pad[i/2].button;
        if(fprintf(dest,"%s 2 ",pad_keys[i])<0)ok=0;ah_obj *a=ah_array_new(vm);
        for(int k=0;k<count;k++){if(fprintf(dest,"%s%d",k?",":"",values[k])<0)ok=0;ah_array_push(vm,a,ah_int(values[k]));}
        if(fputc('\n',dest)==EOF)ok=0;ah_set_member(vm,option,pad_keys[i],ah_objv(a));
    }
    if(fclose(dest))ok=0;if(ok)ok=replace_file(tmp,path);
    if(ok){
        gt4_save_init(saves);
        if(gt4_save_active())ok=gt4_save_capture(vm,game,1) && gt4_save_store();
        gt4_save_shutdown();
    }
    ah_vm_free(vm);return ok;
}
