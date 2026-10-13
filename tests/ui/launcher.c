/* Standalone PC launcher contract, intentionally not an original UI oracle. */
#include "launcher.h"
#include "launcher_photos.h"
#include "launcher_icons.h"
#include "miniz.h"
#include "save.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"launcher line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static ah_obj *game(ah_vm *vm){ah_obj *g=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);ah_set_member(vm,ah_root(vm),"game",ah_objv(g));return g;}
int main(int argc,char **argv)
{
    if(argc>1 && !strcmp(argv[1],"--launcher")){
        launcher_startup start={0};int status=launcher_bootstrap(&argc,&argv,&start);
        if(status==1){CHECK(argc==6 && argv[6]==NULL);printf("BOOT %s | %s | %s\n",argv[1],argv[3],argv[5]);}
        return status<0?1:0;
    }
    CHECK(argc>=2);CHECK(gt4_save_mkdir(argv[1]));
    char prefs[1024],video[1024],saves[1024],options[1040],wrong[1024],message[256];
    snprintf(prefs,sizeof prefs,"%s/launcher.ini",argv[1]);
    snprintf(video,sizeof video,"%s/video.ini",argv[1]);snprintf(options,sizeof options,"%s.game_options",video);
    snprintf(saves,sizeof saves,"%s/saves",argv[1]);snprintf(wrong,sizeof wrong,"%s/wrong.iso",argv[1]);
    FILE *f=fopen(wrong,"wb");CHECK(f);fputs("Not an ISO\n",f);fclose(f);
    CHECK(!launcher_validate_disc(wrong,message,sizeof message));
    CHECK(!launcher_validate_disc("",message,sizeof message));
    if(argc>=3 && strncmp(argv[2],"--",2))CHECK(launcher_validate_disc(argv[2],message,sizeof message));
    remove(prefs); /* Reset only this test-created malformed preferences fixture. */
    launcher_preferences p,q;
    CHECK(launcher_preferences_load(&p,prefs));strcpy(p.disc,"D:/Games/Café Ñandú.iso");strcpy(p.saves,saves);p.show=0;
    CHECK(launcher_preferences_save(&p));CHECK(launcher_preferences_load(&q,prefs));
    CHECK(!strcmp(p.disc,q.disc) && !strcmp(p.saves,q.saves) && !q.show);
    strcpy(p.disc,"bad\npath");CHECK(!launcher_preferences_save(&p));
    CHECK(launcher_preferences_load(&q,prefs) && !strcmp(q.disc,"D:/Games/Café Ñandú.iso"));
    f=fopen(prefs,"wb");CHECK(f);fputs("disc=",f);for(int i=0;i<2000;i++)fputc('x',f);fclose(f);
    CHECK(!launcher_preferences_load(&q,prefs) && q.show && !q.disc[0]);
    launcher_options o,read;launcher_options_defaults(&o);
    o.width=1920;o.height=1080;o.aspect=1;o.volume[0]=64;o.volume[1]=0;
    o.pad[0].button[GT4_PAD_CROSS]=GT4_PAD_BRAKE;o.pad[0].button[GT4_PAD_SQUARE]=GT4_PAD_ACCEL;
    o.pad[0].analog[GT4_PAD_RX]=GT4_PAD_ACCEL;
    f=fopen(options,"wb");CHECK(f);fputs("unit_velocity 1 KMH\ncustom_future_key 0 123\n",f);fclose(f);
    CHECK(launcher_options_save(&o,video,saves,15));CHECK(launcher_options_load(&read,video,saves));
    CHECK(!memcmp(&o,&read,sizeof o));f=fopen(options,"rb");CHECK(f);char text[4096];size_t n=fread(text,1,sizeof text-1,f);text[n]=0;fclose(f);
    CHECK(strstr(text,"custom_future_key 0 123") && strstr(text,"unit_velocity 1 KMH"));
    /* Invalid edits cannot replace a valid file. */
    o.width=0;CHECK(!launcher_options_save(&o,video,saves,1));CHECK(launcher_options_load(&read,video,saves) && read.width==1920);o.width=1920;
    /* Active career keeps its money/unrelated options while edited audio/pad
       take precedence over that career's previous configuration, across load. */
    gt4_save_init(saves);if(!gt4_save_active())CHECK(gt4_save_create(1,"Launcher test"));
    ah_vm *vm=ah_vm_new();ah_obj *g=game(vm),*opt=ah_module_new(vm,"option",g,NULL,NULL);
    ah_set_member(vm,g,"money",ah_int(123456));ah_set_member(vm,g,"option",ah_objv(opt));
    ah_set_member(vm,opt,"unit_velocity",ah_str(vm,"MPH"));
    ah_set_member(vm,opt,"race_bgm_volume_raw",ah_int(12));
    CHECK(gt4_save_capture(vm,g,1) && gt4_save_store());gt4_save_shutdown();ah_vm_free(vm);
    CHECK(launcher_options_load(&read,video,saves) && read.volume[2]==12);
    o.volume[2]=77;CHECK(launcher_options_save(&o,video,saves,6));
    gt4_save_init(saves);CHECK(gt4_save_option_number("race_bgm_volume_raw",-1)==77);
    int keys[16];CHECK(gt4_save_option_array_get("pad_buttons_1p",keys,16) && keys[GT4_PAD_SQUARE]==GT4_PAD_ACCEL);
    vm=ah_vm_new();g=game(vm);gt4_save_restore(vm,g);ah_val v;
    CHECK(ah_get_member(vm,g,"money",&v) && ah_num(v)==123456);
    CHECK(ah_get_member(vm,g,"option",&v));opt=ah_as_obj(v,AO_MODULE);
    CHECK(ah_get_member(vm,opt,"unit_velocity",&v) && !strcmp(ah_cstr(v),"MPH"));
    ah_vm_free(vm);gt4_save_shutdown();
    CHECK(launcher_options_load(&read,video,saves) && read.volume[2]==77);
    int expected=77;if(argc>2 && !strcmp(argv[argc-1],"--negative"))expected++;
    CHECK(read.volume[2]==77);
    /* Local PNG slideshow requires no ISO: skip damaged photos, dissolve,
       wrap and retain only two scaled frames. Fixtures use the real decoder. */
    char gallery[1024];snprintf(gallery,sizeof gallery,"%s/photos",argv[1]);CHECK(gt4_save_mkdir(gallery));
    for(int i=0;i<2;i++){
        uint8_t rgba[4]={i?0:255,0,i?255:0,255};size_t bytes;void *png=tdefl_write_image_to_png_file_in_memory(rgba,1,1,4,&bytes);
        char path[1100];snprintf(path,sizeof path,"%s/%d.png",gallery,i);CHECK(png && plat_file_write(path,png,bytes));mz_free(png);
    }
    char damaged[1100];snprintf(damaged,sizeof damaged,"%s/bad.png",gallery);CHECK(plat_file_write(damaged,"bad",3));
    launcher_photos photos;CHECK(launcher_photos_open(&photos,gallery,"missing-album",2,2));
    uint32_t pixels[4];CHECK(launcher_photos_draw(&photos,pixels));uint32_t first=pixels[0];
    CHECK(first==0xFF0000 || first==0xFF);
    launcher_photos_step(&photos,7.25);CHECK(launcher_photos_draw(&photos,pixels));
    CHECK(((pixels[0]>>16)&255)>=127 && ((pixels[0]>>16)&255)<=128 && (pixels[0]&255)>=127 && (pixels[0]&255)<=128);
    launcher_photos_step(&photos,.75);CHECK(launcher_photos_draw(&photos,pixels) && pixels[0]!=first);
    launcher_photos_step(&photos,8);CHECK(launcher_photos_draw(&photos,pixels) && pixels[0]==first);
    launcher_photos_close(&photos);CHECK(!launcher_photos_open(&photos,"missing","missing",2,2));
    launcher_photos_close(&photos);
#ifdef _WIN32
    /* Exercise the picker/reader encoding with an actual Unicode PNG path. */
    char unicode_dir[1100],unicode_path[1200];
    snprintf(unicode_dir,sizeof unicode_dir,"%s/Café Ñandú",argv[1]);CHECK(gt4_save_mkdir(unicode_dir));
    snprintf(unicode_path,sizeof unicode_path,"%s/foto.png",unicode_dir);
    wchar_t wp[1200];CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,unicode_path,-1,wp,1200));
    uint8_t color[4]={21,42,63,255};size_t png_bytes;void *fixture=tdefl_write_image_to_png_file_in_memory(color,1,1,4,&png_bytes);
    f=_wfopen(wp,L"wb");CHECK(fixture && f);CHECK(fwrite(fixture,1,png_bytes,f)==png_bytes && !fclose(f));mz_free(fixture);
    CHECK(launcher_photos_open(&photos,unicode_dir,"missing",2,2));
    CHECK(launcher_photos_draw(&photos,pixels) && pixels[0]==0x152A3F);launcher_photos_close(&photos);
#endif
#ifdef _WIN32
    /* Real PNG decoder -> cached premultiplied Win32 atlas. Deliberately
       wrong alpha expectation (--icons-negative) proves pixel detection. */
    CHECK(!launcher_icons_create(NULL,80) && !launcher_icons_create(gallery,0));
    char icon_dir[1100],icon_path[1200];snprintf(icon_dir,sizeof icon_dir,"%s/icons",argv[1]);CHECK(gt4_save_mkdir(icon_dir));
    snprintf(icon_path,sizeof icon_path,"%s/devices-media-optical.png",icon_dir);
    uint8_t translucent[4]={200,100,50,128};size_t icon_bytes;
    void *icon_png=tdefl_write_image_to_png_file_in_memory(translucent,1,1,4,&icon_bytes);
    CHECK(icon_png && plat_file_write(icon_path,icon_png,icon_bytes));mz_free(icon_png);
    snprintf(icon_path,sizeof icon_path,"%s/devices-video-display.png",icon_dir);CHECK(plat_file_write(icon_path,"damaged",7));
    HBITMAP atlas=launcher_icons_create(icon_dir,16);CHECK(atlas);BITMAP bitmap;
    CHECK(GetObjectW(atlas,sizeof bitmap,&bitmap) && bitmap.bmWidth==128 && bitmap.bmHeight==16);
    uint32_t *icon_pixels=bitmap.bmBits;CHECK(icon_pixels);
    uint32_t pixel_expected=0x80643219;
    if(argc>2 && !strcmp(argv[argc-1],"--icons-negative"))pixel_expected^=0x01000000;
    CHECK(icon_pixels[16]==pixel_expected && icon_pixels[15*128+31]==pixel_expected);
    CHECK(icon_pixels[0]==0 && icon_pixels[32]==0); /* Missing and damaged stay transparent. */
    DeleteObject(atlas);
    /* Fully transparent bright RGB must not leak into composited edges. */
    translucent[3]=0;icon_png=tdefl_write_image_to_png_file_in_memory(translucent,1,1,4,&icon_bytes);
    snprintf(icon_path,sizeof icon_path,"%s/devices-media-optical.png",icon_dir);
    CHECK(icon_png && plat_file_write(icon_path,icon_png,icon_bytes));mz_free(icon_png);
    atlas=launcher_icons_create(icon_dir,16);CHECK(atlas && GetObjectW(atlas,sizeof bitmap,&bitmap));
    CHECK(((uint32_t *)bitmap.bmBits)[16]==0);DeleteObject(atlas);
#endif
    CHECK(read.volume[2]==expected);
    puts("PASS launcher: no-disc configuration, ISO identity, paths, corrupt preferences, atomic validation, unknown-option preservation and career merge.");
    return 0;
}
