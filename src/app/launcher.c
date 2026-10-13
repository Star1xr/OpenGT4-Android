/* P: standalone Windows preparation. No disc font, image, menu script, audio
   bank or logo is loaded here. Layout and system font are P: new PC choices, not claimed as PS2 fidelity.
   Flat palette references /projects/option/option.mproject ScreenRoot:
   bg 171,171,178/205; popup focus 184,132,46/222; tooltip 165,165,167.
   Shapes are newly drawn GDI primitives, never extracted button textures. */
#include "launcher.h"
#include "launcher_photos.h"
#include "launcher_icons.h"
#include "save.h"
#include "miniz.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <windowsx.h>

enum { HOME,VIDEO,AUDIO,CONTROLS };
enum { ROW=100,BACK=200,APPLY,DEFAULTS,TEST_AUDIO,PORT };
typedef struct {
    launcher_startup *start;
    launcher_options current,draft;
    HWND window,rows[8],fields[24],labels[24],footer[4],port_choice;
    HFONT normal,title,font_small,section;
    wchar_t label_text[24][80];
    RECT label_rect[24];
    HBRUSH panel;
    HBITMAP backdrop,icons,large_icons;
    uint32_t *backdrop_pixels;
    launcher_photos photos;
    ULONGLONG last_tick;
    int page,selection,port,count,result,sdl_ready,hover,pad_mode;
    unsigned nav_previous;
    char status[256],device[192];
    SDL_GameController *pads[2];
    SDL_AudioDeviceID audio;
    float tone[24000];
} launcher_window;
static const wchar_t *const row_names[]={L"Play",L"Game disc",L"Video",L"Audio",L"Controls",L"Player data",L"Show at startup",L"Exit"};
static const wchar_t *const descriptions[]={
 L"Start the game with your selected disc and settings.",
 L"Locate your own disc image. It stays in its current folder.",
 L"Configure your display before starting the game.",
 L"Set music and sound volumes, and test the audio device.",
 L"Assign controller buttons and sticks for players 1 and 2.",
 L"Choose the folder used for profiles, replays and photographs.",
 L"Remember this screen on startup. Open it again with --launcher.",
 L"Close OpenGT4."};
/* Same function ordering as option.ad KeyconfigRoot/DS2 L17..88. New PC
   controls present names rather than using the disc's controller glyphs. */
static const int binding_actions[]={-2,GT4_PAD_ACCEL,GT4_PAD_BRAKE,GT4_PAD_REVERSE,GT4_PAD_SIDE,
    GT4_PAD_SHIFTUP,GT4_PAD_SHIFTDOWN,GT4_PAD_BACKVIEW,GT4_PAD_PLAYVIEW,GT4_PAD_SPECIAL,GT4_PAD_GHOST};
static const wchar_t *const binding_names[]={L"Steering",L"Gas",L"Brake",L"Reverse",L"E-brake",
    L"(MT) Upshift",L"(MT) Downshift",L"Rear View",L"Change View",L"Special",L"Ghost"};
static const wchar_t *const key_choices[]={L"NONE",L"SELECT / Back",L"L3",L"R3",L"START",
    L"up",L"right",L"down",L"left",L"L2 / LT",L"R2 / RT",L"L1 / LB",L"R1 / RB",
    L"Triangle / Y",L"Circle / B",L"Cross / A",L"Square / X",
    L"left analog stick (UP)",L"left analog stick (DOWN)",L"right analog stick (UP)",L"right analog stick (DOWN)"};
static const int resolutions[][2]={{1280,720},{1280,960},{1600,900},{1920,1080},{2560,1440},{3840,2160}};
static int wide(const char *text,wchar_t *out,int size){return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,out,size);}
static void paint_text(HDC dc,HFONT font,int x,int y,int w,int h,COLORREF color,const wchar_t *text,UINT flags)
{
    HFONT old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
    RECT r={x,y,x+w,y+h};DrawTextW(dc,text,-1,&r,flags|DT_NOPREFIX);SelectObject(dc,old);
}
static void gradient(HDC dc,RECT r,COLORREF top,COLORREF bottom)
{
    int h=r.bottom-r.top;if(h<1)return;
    for(int y=0;y<h;y++){
        int red=(GetRValue(top)*(h-y)+GetRValue(bottom)*y)/h;
        int green=(GetGValue(top)*(h-y)+GetGValue(bottom)*y)/h;
        int blue=(GetBValue(top)*(h-y)+GetBValue(bottom)*y)/h;
        HBRUSH b=CreateSolidBrush(RGB(red,green,blue));RECT line={r.left,r.top+y,r.right,r.top+y+1};
        FillRect(dc,&line,b);DeleteObject(b);
    }
}
static void gradient_h(HDC dc,RECT r,COLORREF left,COLORREF right)
{
    int w=r.right-r.left;if(w<1)return;
    for(int x=0;x<w;x++){
        int red=(GetRValue(left)*(w-x)+GetRValue(right)*x)/w,green=(GetGValue(left)*(w-x)+GetGValue(right)*x)/w,blue=(GetBValue(left)*(w-x)+GetBValue(right)*x)/w;
        HBRUSH b=CreateSolidBrush(RGB(red,green,blue));RECT line={r.left+x,r.top,r.left+x+1,r.bottom};FillRect(dc,&line,b);DeleteObject(b);
    }
}
static HWND control(launcher_window *s,const wchar_t *cls,const wchar_t *name,DWORD style,int id,int x,int y,int w,int h)
{
    HWND win=CreateWindowExW(0,cls,name,WS_CHILD|WS_VISIBLE|style,x,y,w,h,s->window,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    SendMessageW(win,WM_SETFONT,(WPARAM)s->normal,TRUE);return win;
}
static void destroy_page(launcher_window *s)
{
    for(int i=0;i<24;i++){if(s->fields[i])DestroyWindow(s->fields[i]);if(s->labels[i])DestroyWindow(s->labels[i]);s->fields[i]=s->labels[i]=NULL;s->label_text[i][0]=0;}
    for(int i=0;i<4;i++){if(s->footer[i])DestroyWindow(s->footer[i]);s->footer[i]=NULL;}
    if(s->port_choice)DestroyWindow(s->port_choice);s->port_choice=NULL;s->count=0;
}
static void set_label(launcher_window *s,int k,const wchar_t *text,int x,int y,int w,int h)
{
    wcsncpy(s->label_text[k],text,79);s->label_text[k][79]=0;
    s->label_rect[k]=(RECT){x,y,x+w,y+h};
}
static void combo(launcher_window *s,int k,const wchar_t *label,const wchar_t *const *choices,int n,int selected,int x,int y,int width)
{
    set_label(s,k,label,x,y,width,24);
    s->fields[k]=control(s,L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,300+k,x,y+25,width,300);
    for(int i=0;i<n;i++)SendMessageW(s->fields[k],CB_ADDSTRING,0,(LPARAM)choices[i]);
    SendMessageW(s->fields[k],CB_SETCURSEL,selected,0);
}
static void page(launcher_window *s,int which)
{
    destroy_page(s);s->page=which;s->draft=s->current;
    for(int i=0;i<8;i++)ShowWindow(s->rows[i],which==HOME?SW_SHOW:SW_HIDE);
    if(which!=HOME){
        s->footer[0]=control(s,L"BUTTON",L"Cancel",BS_OWNERDRAW|WS_TABSTOP,BACK,112,592,192,34);
        s->footer[1]=control(s,L"BUTTON",L"OK",BS_OWNERDRAW|WS_TABSTOP,APPLY,588,592,192,34);
        s->footer[2]=control(s,L"BUTTON",L"Default",BS_OWNERDRAW|WS_TABSTOP,DEFAULTS,350,592,192,34);
    }
    if(which==VIDEO){
        const wchar_t *ratio[]={L"Original 4:3",L"Widescreen 16:9"},*display[]={L"Windowed",L"Full screen"},*on[]={L"Off",L"On"};
        const wchar_t *res[7];wchar_t text[7][64];int selected=6;
        for(int i=0;i<6;i++){swprintf(text[i],64,L"%d x %d",resolutions[i][0],resolutions[i][1]);res[i]=text[i];if(s->draft.width==resolutions[i][0] && s->draft.height==resolutions[i][1])selected=i;}
        swprintf(text[6],64,L"%d x %d (current)",s->draft.width,s->draft.height);res[6]=text[6];
        const wchar_t *filters[]={L"Smooth",L"Sharp",L"Original pixels"};
        const wchar_t *sharp[]={L"0%",L"25%",L"50%",L"75%",L"100%"};
        const wchar_t *scale[]={L"Native resolution",L"FSR Ultra quality",L"FSR Quality",L"FSR Balanced",L"FSR Performance"};
        combo(s,0,L"Aspect ratio",ratio,2,s->draft.aspect,348,194,218);
        combo(s,1,L"Display mode",display,2,s->draft.fullscreen,590,194,218);
        combo(s,2,L"Resolution",res,7,selected,348,294,218);
        combo(s,3,L"Vertical synchronization",on,2,s->draft.vsync,590,294,218);
        combo(s,4,L"Interface filtering",filters,3,s->draft.filter,348,394,218);
        combo(s,5,L"Text clarity",sharp,5,(s->draft.text+12)/25,590,394,218);
        combo(s,6,L"3D scaling",scale,5,s->draft.upscale,348,494,218);s->count=7;
    }else if(which==AUDIO){
        const wchar_t *labels[]={L"Menu music",L"Menu effects",L"Race music",L"Race effects",L"Replay music",L"Replay effects",L"Slide show music"};
        const wchar_t *choices[129];wchar_t text[129][24];
        for(int i=0;i<=128;i++){swprintf(text[i],24,L"%d%%",(i*100+64)/128);choices[i]=text[i];}
        for(int i=0;i<7;i++)combo(s,i,labels[i],choices,129,s->draft.volume[i],348+(i%2)*242,190+(i/2)*95,218);
        s->footer[3]=control(s,L"BUTTON",L"Test audio",BS_OWNERDRAW|WS_TABSTOP,TEST_AUDIO,590,527,218,34);s->count=7;
    }else if(which==CONTROLS){
        s->port_choice=control(s,L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,PORT,657,120,150,100);
        const wchar_t *ports[]={L"Player 1",L"Player 2"};
        for(int i=0;i<2;i++)SendMessageW(s->port_choice,CB_ADDSTRING,0,(LPARAM)ports[i]);
        SendMessageW(s->port_choice,CB_SETCURSEL,s->port,0);
        gt4_pad_config *c=&s->draft.pad[s->port];
        for(int i=0;i<11;i++){
            int y=174+i*36,select=0;
            set_label(s,i,binding_names[i],108,y+5,275,28);
            s->fields[i]=control(s,L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP|WS_VSCROLL,300+i,406,y,374,400);
            if(!i){
                const wchar_t *steering[]={L"NONE",L"left analog stick",L"right analog stick",L"left / right buttons"};
                for(int k=0;k<4;k++)SendMessageW(s->fields[i],CB_ADDSTRING,0,(LPARAM)steering[k]);
                if(c->analog[GT4_PAD_LX]==-2)select=1;
                else if(c->analog[GT4_PAD_RX]==-2)select=2;
                else if(c->button[GT4_PAD_LEFT]==GT4_PAD_LSTEER && c->button[GT4_PAD_RIGHT]==GT4_PAD_RSTEER)select=3;
            }else{
                for(int k=0;k<21;k++)SendMessageW(s->fields[i],CB_ADDSTRING,0,(LPARAM)key_choices[k]);
                for(int k=0;k<16;k++)if(c->button[k]==binding_actions[i]){select=k+1;break;}
                if(!select)for(int k=2;k<6;k++)if(c->analog[k]==binding_actions[i]){select=k+15;break;}
            }
            SendMessageW(s->fields[i],CB_SETCURSEL,select,0);
        }
        s->count=11;
    }
    InvalidateRect(s->window,NULL,FALSE);
    SetFocus(which==HOME?s->rows[s->selection]:s->fields[0]);
}
static int value(launcher_window *s,int n){return (int)SendMessageW(s->fields[n],CB_GETCURSEL,0,0);}
static int apply(launcher_window *s)
{
    int groups=0;
    if(s->page==VIDEO){
        s->draft.aspect=value(s,0);s->draft.fullscreen=value(s,1);int r=value(s,2);
        if(r<6){s->draft.width=resolutions[r][0];s->draft.height=resolutions[r][1];}
        s->draft.vsync=value(s,3);s->draft.filter=value(s,4);s->draft.text=value(s,5)*25;s->draft.upscale=value(s,6);groups=1;
    }else if(s->page==AUDIO){for(int i=0;i<7;i++)s->draft.volume[i]=value(s,i);groups=2;}
    else if(s->page==CONTROLS){
        int selected[11];for(int i=0;i<11;i++)selected[i]=value(s,i);
        for(int i=1;i<11;i++)for(int j=i+1;j<11;j++)if(selected[i] && selected[i]==selected[j]){
            strcpy(s->status,"Two functions use the same button. Choose a different assignment.");
            InvalidateRect(s->window,NULL,FALSE);return 0;
        }
        if(selected[0]==3)for(int i=1;i<11;i++)if(selected[i]==GT4_PAD_LEFT+1 || selected[i]==GT4_PAD_RIGHT+1){
            strcpy(s->status,"Left / right buttons are assigned to steering. Choose another button.");
            InvalidateRect(s->window,NULL,FALSE);return 0;
        }
        gt4_pad_config *c=&s->draft.pad[s->port];
        for(int i=0;i<11;i++){
            int action=binding_actions[i];
            for(int k=0;k<16;k++)if(i?c->button[k]==action:c->button[k]==GT4_PAD_LSTEER || c->button[k]==GT4_PAD_RSTEER)c->button[k]=-1;
            for(int k=0;k<6;k++)if(c->analog[k]==action)c->analog[k]=-1;
        }
        if(selected[0]==1)c->analog[GT4_PAD_LX]=-2;
        if(selected[0]==2)c->analog[GT4_PAD_RX]=-2;
        /* The original DS2 default also enables digital left/right alongside
           the stick. Retain that available fallback for the new PC editor. */
        if(selected[0]){c->button[GT4_PAD_LEFT]=GT4_PAD_LSTEER;c->button[GT4_PAD_RIGHT]=GT4_PAD_RSTEER;}
        for(int i=1;i<11;i++){
            if(selected[i]>=1 && selected[i]<=16)c->button[selected[i]-1]=binding_actions[i];
            if(selected[i]>=17)c->analog[selected[i]-15]=binding_actions[i];
        }
        groups=4<<s->port;
    }
    if(!groups)return 0;
    int ok=launcher_options_save(&s->draft,s->start->video_file,s->start->preferences.saves,groups);
    if(ok){s->current=s->draft;strcpy(s->status,"Settings saved.");page(s,HOME);}
    else {strcpy(s->status,"Could not save all settings. Check access to the settings and player folders.");MessageBoxW(s->window,L"Could not save all settings. Check folder permissions.",L"OpenGT4",MB_ICONERROR);}
    return ok;
}
static int choose_path(launcher_window *s,char *out,int folder)
{
    wchar_t selected[LAUNCHER_PATH]=L"";
    if(folder){
        BROWSEINFOW b={0};b.hwndOwner=s->window;b.lpszTitle=L"Choose the OpenGT4 player data folder";
        b.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;PIDLIST_ABSOLUTE id=SHBrowseForFolderW(&b);
        int ok=id && SHGetPathFromIDListW(id,selected);CoTaskMemFree(id);if(!ok)return 0;
    }else{
        OPENFILENAMEW open={0};open.lStructSize=sizeof open;open.hwndOwner=s->window;
        open.lpstrFilter=L"PlayStation 2 disc image (*.iso)\0*.iso\0All files\0*.*\0";
        open.lpstrFile=selected;open.nMaxFile=LAUNCHER_PATH;open.lpstrTitle=L"OpenGT4 - Select game disc";
        open.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(!GetOpenFileNameW(&open))return 0;
    }
    return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,selected,-1,out,LAUNCHER_PATH,NULL,NULL)!=0;
}
static void load_photos(launcher_window *s);
static void pad_navigate(launcher_window *s,int key);
static void activate(launcher_window *s,int n)
{
    char selected[LAUNCHER_PATH];s->selection=n;
    if(n==0){
        if(!launcher_validate_disc(s->start->preferences.disc,s->status,sizeof s->status)){InvalidateRect(s->window,NULL,FALSE);return;}
        if(!launcher_preferences_save(&s->start->preferences)){strcpy(s->status,"Could not save launcher preferences.");InvalidateRect(s->window,NULL,FALSE);return;}
        s->result=1;DestroyWindow(s->window);
    }else if(n==1 || n==5){
        if(choose_path(s,selected,n==5)){
            launcher_preferences draft=s->start->preferences;
            if(n==1){
                if(!launcher_validate_disc(selected,s->status,sizeof s->status)){InvalidateRect(s->window,NULL,FALSE);return;}
                strcpy(draft.disc,selected);
            }else strcpy(draft.saves,selected);
            if(launcher_preferences_save(&draft)){
                s->start->preferences=draft;
                launcher_options_load(&s->current,s->start->video_file,draft.saves);
                if(n==5){load_photos(s);}if(n==5)strcpy(s->status,"Player data folder selected. Existing files stay in place.");
            }else strcpy(s->status,"Could not save launcher preferences.");
            InvalidateRect(s->window,NULL,FALSE);
        }
    }else if(n>=2 && n<=4)page(s,n-1);
    else if(n==6){
        launcher_preferences p=s->start->preferences;p.show=!p.show;
        if(launcher_preferences_save(&p)){s->start->preferences=p;strcpy(s->status,"Startup preference saved.");}
        else strcpy(s->status,"Could not save launcher preferences.");
        InvalidateRect(s->rows[6],NULL,TRUE);InvalidateRect(s->window,NULL,FALSE);
    }else DestroyWindow(s->window);
}
static void test_audio(launcher_window *s)
{
    if(s->audio){SDL_CloseAudioDevice(s->audio);s->audio=0;}
    if(!(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO)<0)goto failed;
    SDL_AudioSpec want={0},got;want.freq=48000;want.format=AUDIO_F32SYS;want.channels=1;want.samples=1024;
    s->audio=SDL_OpenAudioDevice(NULL,0,&want,&got,0);if(!s->audio)goto failed;
    /* P: generated device-check tone, never an imitation of a game asset. */
    float gain=(float)value(s,1)/128.f;
    for(int i=0;i<24000;i++){float fade=(float)fmin(i/480.0,(24000-i)/2400.0);if(fade>1)fade=1;s->tone[i]=.15f*gain*fade*(float)sin(i*6.283185307179586*440/48000);}
    if(SDL_QueueAudio(s->audio,s->tone,sizeof s->tone)<0)goto failed;
    SDL_PauseAudioDevice(s->audio,0);strcpy(s->status,"Device test: generated tone at the menu effects volume.");InvalidateRect(s->window,NULL,FALSE);return;
failed:
    snprintf(s->status,sizeof s->status,"Audio device unavailable: %.160s",SDL_GetError());InvalidateRect(s->window,NULL,FALSE);
}
static void controller_tick(launcher_window *s)
{
    SDL_Event e;while(SDL_PollEvent(&e)){}
    for(int p=0;p<2;p++)if(s->pads[p] && !SDL_GameControllerGetAttached(s->pads[p])){SDL_GameControllerClose(s->pads[p]);s->pads[p]=NULL;}
    for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)){
        SDL_JoystickID id=SDL_JoystickGetDeviceInstanceID(i);int used=0;
        for(int p=0;p<2;p++)if(s->pads[p] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(s->pads[p]))==id)used=1;
        if(!used)for(int p=0;p<2;p++)if(!s->pads[p]){s->pads[p]=SDL_GameControllerOpen(i);break;}
    }
    SDL_GameController *nav=s->pads[0];unsigned buttons=0;
    const SDL_GameControllerButton navigation[]={SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B};
    const int keys[]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_RETURN,VK_ESCAPE};
    if(nav)for(int i=0;i<6;i++)if(SDL_GameControllerGetButton(nav,navigation[i]))buttons|=1u<<i;
    unsigned pressed=buttons & ~s->nav_previous;s->nav_previous=buttons;
    if(GetForegroundWindow()==s->window)for(int i=0;i<6;i++)if(pressed&(1u<<i)){
        if(s->page==HOME && i<4){pad_navigate(s,keys[i]);continue;}
        if(s->page==HOME && i==4){activate(s,s->hover>=0?s->hover:s->selection);continue;}
        HWND focus=GetFocus();PostMessageW(focus?focus:s->window,WM_KEYDOWN,keys[i],0);
    }
    SDL_GameController *pad=s->pads[s->port];char before[192];strcpy(before,s->device);
    if(pad)snprintf(s->device,sizeof s->device,"%.80s     LX %d   LY %d   LT %d   RT %d",SDL_GameControllerName(pad),
        SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX),SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY),
        SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT),SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
    else strcpy(s->device,"No controller connected. Keyboard and mouse are available.");
    if(strcmp(before,s->device) && s->page==CONTROLS){RECT r={80,647,820,678};InvalidateRect(s->window,&r,FALSE);}
}
static void load_photos(launcher_window *s)
{
    launcher_photos_close(&s->photos);
    char base[LAUNCHER_PATH],cache[LAUNCHER_PATH+64],album[LAUNCHER_PATH+16];
    strcpy(base,s->start->preferences.file);char *a=strrchr(base,'/'),*b=strrchr(base,'\\'),*end=a;
    if(b && (!a || b>a))end=b;if(end)*end=0;else strcpy(base,".");
    snprintf(cache,sizeof cache,"%s/launcher_photos/photos",base);
    snprintf(album,sizeof album,"%s/photos",s->start->preferences.saves);
    launcher_photos_open(&s->photos,cache,album,900,700);
}
static void compose_backdrop(launcher_window *s,HDC dc)
{
    if(!s->backdrop){
        BITMAPINFO info={0};info.bmiHeader.biSize=sizeof info.bmiHeader;info.bmiHeader.biWidth=900;
        info.bmiHeader.biHeight=-700;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        s->backdrop=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&s->backdrop_pixels,NULL,0);
    }
    if(!s->backdrop)return;
    int photo=launcher_photos_draw(&s->photos,s->backdrop_pixels);
    /* P: backdrop treatment and offset home composition. Authored ScreenRoot
       flat tints, newly drawn panels/capsules; no source images are embedded. */
    for(int y=0;y<700;y++)for(int x=0;x<900;x++){
        /* the Options screen's field: 140 in the middle to 75 at the edges */
        int dx=x-450,dy=y-330,edge=(dx*dx/(450*450/100)+dy*dy/(330*330/100));
        int base=s->page==HOME?140-edge*65/200+(((x/8+y/8)&1)?3:0):88-y*42/700+(((x/8+y/8)&1)?4:0);
        if(base<70)base=70;
        uint32_t source=photo?s->backdrop_pixels[y*900+x]:(uint32_t)(base|(base<<8)|(base<<16));
        int red=(source>>16)&255,green=(source>>8)&255,blue=source&255;
        int veil=s->page==HOME?0:y<170?(170-y)*100/170:y>590?(y-590)*170/110:0;
        red=red*(255-veil)/255;green=green*(255-veil)/255;blue=blue*(255-veil)/255;
        int panel=s->page==HOME?(x>=568 && x<860 && y>=182 && y<520):(x>=80 && x<820 && y>=118 && y<632);
        if(panel){
            int tint=171,alpha=205;
            if(s->page!=HOME && y>=170 && y<570 && ((y-170)/36)&1){tint=154;alpha=183;}
            red=(tint*alpha+red*(255-alpha))/255;green=(tint*alpha+green*(255-alpha))/255;blue=((tint+7)*alpha+blue*(255-alpha))/255;
        }
        s->backdrop_pixels[y*900+x]=(uint32_t)(blue|(green<<8)|(red<<16));
    }
}
static void draw(launcher_window *s,HDC dc)
{
    compose_backdrop(s,dc);
    if(s->backdrop){HDC src=CreateCompatibleDC(dc);HBITMAP old=SelectObject(src,s->backdrop);BitBlt(dc,0,0,900,700,src,0,0,SRCCOPY);SelectObject(src,old);DeleteDC(src);}
    const wchar_t *titles[]={L"",L"Screen settings",L"Sound settings",L"Controller button config"};
    if(s->page==HOME){
        /* the Options screen's own composition (option.mproject OptionRoot):
           the page's title centred in white over a dark shadow, sections
           under a light-to-dark bar with an italic title, the objects as
           tiles, a popup of the game's own palette for the choice's
           details, the black help line and the green exit at the foot */
        paint_text(dc,s->title,2,24,900,56,RGB(40,40,44),L"OpenGT4",DT_SINGLELINE|DT_CENTER);
        paint_text(dc,s->title,0,22,900,56,RGB(246,246,246),L"OpenGT4",DT_SINGLELINE|DT_CENTER);
        const wchar_t *sections[]={L"Game",L"Settings"};
        for(int k=0;k<2;k++){
            int y=k?330:112;RECT bar={36,y,548,y+36};
            gradient_h(dc,bar,RGB(158,157,153),RGB(112,111,110));
            HBRUSH line=CreateSolidBrush(RGB(84,82,80));RECT under={36,y+35,548,y+36};FillRect(dc,&under,line);DeleteObject(line);
            paint_text(dc,s->section,52,y+2,400,34,RGB(60,58,56),sections[k],DT_SINGLELINE|DT_VCENTER);
            paint_text(dc,s->section,50,y,400,34,RGB(250,250,250),sections[k],DT_SINGLELINE|DT_VCENTER);
        }
        /* the details popup: the dark title bar and the grey body of the
           game's dialogs (41,34,29 / 171,171,178) */
        const wchar_t *heads[]={L"Ready to drive",L"Game disc",L"Screen settings",L"Sound settings",
            L"Controller configuration",L"Player data",L"Startup",L"Exit"};
        RECT heading={568,148,860,182};gradient(dc,heading,RGB(62,58,56),RGB(41,34,29));
        paint_text(dc,s->normal,582,148,270,34,RGB(218,218,222),heads[s->selection],DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        char lines[3][LAUNCHER_PATH];int n=0,state=-1;
        const char *disc=s->start->preferences.disc;
        const char *a=strrchr(disc,'/'),*b=strrchr(disc,'\\');if(b && (!a || b>a))a=b;const char *file=a?a+1:disc;
        wchar_t wdisc[LAUNCHER_PATH];DWORD attributes=*disc && wide(disc,wdisc,LAUNCHER_PATH)?GetFileAttributesW(wdisc):INVALID_FILE_ATTRIBUTES;
        int present=attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY);
        switch(s->selection){
        case 0:case 1:state=present;
            snprintf(lines[n++],LAUNCHER_PATH,"%s",present?"Disc image found.":*disc?"The disc image is missing.":"No disc image selected.");
            snprintf(lines[n++],LAUNCHER_PATH,"%s",*disc?file:"Choose Game disc to locate your own copy.");
            snprintf(lines[n++],LAUNCHER_PATH,"NTSC-U  /  SCUS-97328");break;
        case 2:snprintf(lines[n++],LAUNCHER_PATH,"%d x %d  /  %s",s->current.width,s->current.height,s->current.fullscreen?"Full screen":"Windowed");
            snprintf(lines[n++],LAUNCHER_PATH,"%s  /  V-sync %s",s->current.aspect?"16:9":"4:3",s->current.vsync?"On":"Off");break;
        case 3:snprintf(lines[n++],LAUNCHER_PATH,"Music and sound effects");snprintf(lines[n++],LAUNCHER_PATH,"Menu / Race / Replay / Slide show");break;
        case 4:snprintf(lines[n++],LAUNCHER_PATH,"Default controls");
            snprintf(lines[n++],LAUNCHER_PATH,"Controller: left stick steers, Cross accelerates, Square brakes, Circle handbrake, Triangle reverse, R2 / L2 shift up / down, L1 looks back, Select changes the view, R1 nitro. Start pauses.");
            snprintf(lines[n++],LAUNCHER_PATH,"Keyboard: arrows steer, accelerate and brake, A / Z shift, Space handbrake, V view, Esc pause. Click Controls to change them.");break;
        case 5:snprintf(lines[n++],LAUNCHER_PATH,"%s",s->start->preferences.saves);snprintf(lines[n++],LAUNCHER_PATH,"Profiles / Replays / Photographs");break;
        case 6:snprintf(lines[n++],LAUNCHER_PATH,"Show this screen: %s",s->start->preferences.show?"On":"Off");snprintf(lines[n++],LAUNCHER_PATH,"Open it again with --launcher");break;
        default:snprintf(lines[n++],LAUNCHER_PATH,"Close OpenGT4.");snprintf(lines[n++],LAUNCHER_PATH,"Your saved settings are retained.");break;
        }
        int y=200;
        for(int k=0;k<n;k++){
            wchar_t text[LAUNCHER_PATH];wide(lines[k],text,LAUNCHER_PATH);int x=584;
            if(!k && state>=0){   /* the disc's state: the game's own green and red lamps */
                HBRUSH lamp=CreateSolidBrush(state?RGB(46,160,64):RGB(196,52,40));HPEN pen=CreatePen(PS_SOLID,1,RGB(30,30,30));
                HGDIOBJ ob=SelectObject(dc,lamp),op=SelectObject(dc,pen);Ellipse(dc,584,y+5,598,y+19);
                SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(lamp);DeleteObject(pen);x=606;
            }
            if(k){   /* the details wrap, as the game's popups do */
                RECT box={x,y,854,y+200};HFONT old=SelectObject(dc,s->font_small);
                DrawTextW(dc,text,-1,&box,DT_WORDBREAK|DT_CALCRECT|DT_NOPREFIX);SelectObject(dc,old);
                int height=box.bottom-box.top;if(height<20)height=20;
                paint_text(dc,s->font_small,x,y,854-x,height,RGB(41,34,29),text,DT_WORDBREAK|DT_END_ELLIPSIS);
                y+=height+8;
            }else{
                paint_text(dc,s->normal,x,y,854-x,26,RGB(0,0,0),text,DT_SINGLELINE|DT_END_ELLIPSIS);
                y+=34;
            }
        }
        paint_text(dc,s->font_small,584,452,262,60,RGB(41,34,29),
            L"OpenGT4 needs your own Gran Turismo 4 disc image. No game files are included.",DT_WORDBREAK);
        paint_text(dc,s->font_small,568,526,292,22,RGB(220,220,224),L"Classic",DT_SINGLELINE|DT_RIGHT);
    }else{
        paint_text(dc,s->title,72,36,600,56,RGB(239,239,239),L"OpenGT4",DT_SINGLELINE);
        paint_text(dc,s->font_small,670,65,150,22,RGB(185,186,187),L"CLASSIC",DT_RIGHT);
        RECT bar={80,118,820,153};HBRUSH header=CreateSolidBrush(RGB(41,34,29));FillRect(dc,&bar,header);DeleteObject(header);
        paint_text(dc,s->normal,104,122,600,30,RGB(171,171,178),titles[s->page],DT_SINGLELINE);
    }
    if(s->page==HOME){   /* nothing more: the pages below */
    }else if(s->page!=CONTROLS){
        const wchar_t *info=s->page==VIDEO?L"Set your display before starting the game.\n\nText clarity remains independent of 3D scaling.":
            L"Set music and sound volumes.\n\nTest audio plays a tone. Game music and effects load after Play.";
        launcher_icon_draw(dc,s->large_icons,s->page==VIDEO?2:3,128,179,144);
        paint_text(dc,s->normal,104,342,210,226,RGB(0,0,0),info,DT_WORDBREAK);
    }
    for(int i=0;i<s->count;i++)if(s->label_text[i][0]){
        RECT r=s->label_rect[i];paint_text(dc,s->normal,r.left,r.top,r.right-r.left,r.bottom-r.top,
            RGB(0,0,0),s->label_text[i],DT_SINGLELINE|DT_VCENTER);
    }
    RECT foot={0,647,900,700};HBRUSH black=CreateSolidBrush(RGB(0,0,0));FillRect(dc,&foot,black);DeleteObject(black);
    /* the help line centred, as the game's tooltip bar */
    if(s->page==HOME)paint_text(dc,s->normal,40,654,820,24,RGB(165,165,167),descriptions[s->selection],DT_SINGLELINE|DT_CENTER|DT_END_ELLIPSIS);
    else if(s->page==CONTROLS){wchar_t device[256];wide(s->device,device,256);paint_text(dc,s->font_small,80,650,740,30,RGB(165,165,167),device,DT_SINGLELINE|DT_END_ELLIPSIS);}
    wchar_t status[256];wide(s->status,status,256);
    paint_text(dc,s->font_small,80,680,740,20,RGB(165,165,167),status,DT_SINGLELINE|DT_END_ELLIPSIS);
    /* the GPL's Appropriate Legal Notices (LICENSE, NOTICE) */
    if(s->page==HOME && !s->status[0])paint_text(dc,s->font_small,80,680,740,20,RGB(110,110,112),
        L"OpenGT4 is free software under the GNU GPL v3, with no warranty. See LICENSE.txt and NOTICE.txt.",DT_SINGLELINE|DT_CENTER|DT_END_ELLIPSIS);
}
static void draw_button_direct(launcher_window *s,DRAWITEMSTRUCT *d)
{
    if(d->CtlType==ODT_COMBOBOX){
        wchar_t text[256]=L"";int index=(int)d->itemID;
        if(index<0)index=(int)SendMessageW(d->hwndItem,CB_GETCURSEL,0,0);
        if(index>=0)SendMessageW(d->hwndItem,CB_GETLBTEXT,index,(LPARAM)text);
        HBRUSH b=CreateSolidBrush(d->itemState&ODS_SELECTED?RGB(184,132,46):RGB(170,170,172));
        FillRect(d->hDC,&d->rcItem,b);DeleteObject(b);
        paint_text(d->hDC,s->normal,d->rcItem.left+6,d->rcItem.top,d->rcItem.right-d->rcItem.left-12,d->rcItem.bottom-d->rcItem.top,
            RGB(16,18,20),text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);return;
    }
    if(s->backdrop){
        POINT origin={0,0};MapWindowPoints(d->hwndItem,s->window,&origin,1);
        HDC background=CreateCompatibleDC(d->hDC);HBITMAP old=SelectObject(background,s->backdrop);
        BitBlt(d->hDC,0,0,d->rcItem.right,d->rcItem.bottom,background,origin.x,origin.y,SRCCOPY);
        SelectObject(background,old);DeleteDC(background);
    }
    RECT r=d->rcItem;int focus=(d->itemState&ODS_FOCUS)!=0,pressed=d->itemState&ODS_SELECTED;
    if(d->CtlID>=ROW && d->CtlID<ROW+8){
        int item=d->CtlID-ROW;
        focus=s->hover==item;   /* the mouse lights a tile, as the game's cursor would */
        wchar_t name[128];GetWindowTextW(d->hwndItem,name,128);
        if(item==6){   /* the startup switch: a small box under the tiles */
            RECT check={0,5,16,21};HBRUSH shade=CreateSolidBrush(focus?RGB(184,132,46):RGB(196,196,200));
            FillRect(d->hDC,&check,shade);DeleteObject(shade);
            HBRUSH edge=CreateSolidBrush(RGB(40,40,44));FrameRect(d->hDC,&check,edge);DeleteObject(edge);
            if(s->start->preferences.show){HPEN pen=CreatePen(PS_SOLID,2,RGB(20,22,24)),old=SelectObject(d->hDC,pen);
                POINT tick[]={{3,13},{7,17},{13,8}};Polyline(d->hDC,tick,3);SelectObject(d->hDC,old);DeleteObject(pen);}
            paint_text(d->hDC,s->font_small,25,1,r.right-25,r.bottom,RGB(30,30,32),L"Show this screen at startup",DT_SINGLELINE|DT_VCENTER);
            paint_text(d->hDC,s->font_small,24,0,r.right-25,r.bottom,focus?RGB(240,196,96):RGB(232,232,236),L"Show this screen at startup",DT_SINGLELINE|DT_VCENTER);
            return;
        }
        if(item==7){   /* the green exit of the game's menus */
            int saved=SaveDC(d->hDC);HRGN clip=CreateRoundRectRgn(0,0,r.right+1,r.bottom+1,10,10);SelectClipRgn(d->hDC,clip);
            gradient(d->hDC,r,RGB(96,214,104),RGB(18,150,46));
            RestoreDC(d->hDC,saved);DeleteObject(clip);
            HPEN pen=CreatePen(PS_SOLID,focus?3:1,focus?RGB(184,132,46):RGB(10,90,30));HGDIOBJ op=SelectObject(d->hDC,pen),ob=SelectObject(d->hDC,GetStockObject(NULL_BRUSH));
            RoundRect(d->hDC,0,0,r.right,r.bottom,10,10);SelectObject(d->hDC,op);SelectObject(d->hDC,ob);DeleteObject(pen);
            if(s->icons)launcher_icon_draw(d->hDC,s->icons,7,(r.right-32)/2,(r.bottom-32)/2,32);
            return;
        }
        /* a tile of the Options screen: its dark top strip, the grey body in
           two shades, the object's picture and its name in black */
        int w=r.right-4,h=r.bottom-4;
        RECT shadow={4,4,r.right,r.bottom};HBRUSH sb=CreateSolidBrush(RGB(70,70,74));FillRect(d->hDC,&shadow,sb);DeleteObject(sb);
        RECT top={0,0,w,12};gradient(d->hDC,top,RGB(60,52,48),RGB(42,34,30));
        RECT upper={0,12,w,h*62/100};gradient(d->hDC,upper,RGB(216,216,218),RGB(196,196,200));
        RECT lower={0,h*62/100,w,h};gradient(d->hDC,lower,RGB(164,164,168),RGB(144,144,150));
        HBRUSH frame=CreateSolidBrush(focus?RGB(184,132,46):RGB(112,112,118));RECT body={0,0,w,h};FrameRect(d->hDC,&body,frame);
        if(focus){RECT inner={1,1,w-1,h-1};FrameRect(d->hDC,&inner,frame);RECT inner2={2,2,w-2,h-2};FrameRect(d->hDC,&inner2,frame);}
        DeleteObject(frame);
        if(s->icons)launcher_icon_draw(d->hDC,s->icons,item,(w-80)/2,18,80);
        paint_text(d->hDC,s->normal,0,h*62/100,w,h-h*62/100,RGB(0,0,0),name,DT_SINGLELINE|DT_VCENTER|DT_CENTER);
        if(focus && s->pad_mode){   /* with the controller, the game's yellow arrow cursor */
            int x=w/2+10,y=70;
            POINT arrow[]={{x,y},{x+24,y+12},{x+14,y+14},{x+12,y+24}};
            HBRUSH yellow=CreateSolidBrush(RGB(246,196,40));HPEN dark=CreatePen(PS_SOLID,1,RGB(96,62,0));
            HGDIOBJ ob=SelectObject(d->hDC,yellow),op=SelectObject(d->hDC,dark);Polygon(d->hDC,arrow,4);
            SelectObject(d->hDC,ob);SelectObject(d->hDC,op);DeleteObject(yellow);DeleteObject(dark);
        }
        return;
    }
    int height=r.bottom-r.top;
    /* P: procedural capsule bevel replaces the disc button images. */
    int saved=SaveDC(d->hDC);HRGN clip=CreateRoundRectRgn(r.left,r.top,r.right+1,r.bottom+1,height,height);
    SelectClipRgn(d->hDC,clip);
    COLORREF top=focus?RGB(206,170,104):RGB(203,203,208),bottom=focus?RGB(184,132,46):RGB(154,154,161);
    if(pressed){COLORREF t=top;top=bottom;bottom=t;}gradient(d->hDC,r,top,bottom);
    RECT shine={r.left,r.top+1,r.right,r.top+height/3};gradient(d->hDC,shine,focus?RGB(218,190,130):RGB(222,222,225),top);
    RestoreDC(d->hDC,saved);DeleteObject(clip);
    HPEN pen=CreatePen(PS_SOLID,1,focus?RGB(119,86,32):RGB(117,117,124)),old_pen=SelectObject(d->hDC,pen);
    HBRUSH old_brush=SelectObject(d->hDC,GetStockObject(NULL_BRUSH));RoundRect(d->hDC,r.left,r.top,r.right,r.bottom,height,height);
    SelectObject(d->hDC,old_brush);SelectObject(d->hDC,old_pen);DeleteObject(pen);
    wchar_t name[128];GetWindowTextW(d->hwndItem,name,128);
    if(d->CtlID==ROW+6)swprintf(name,128,L"Show at startup: %s",s->start->preferences.show?L"On":L"Off");
    paint_text(d->hDC,s->normal,r.left+14,r.top,r.right-r.left-28,height,RGB(0,0,0),name,DT_SINGLELINE|DT_VCENTER|DT_CENTER);

}
static void hover(launcher_window *s,int item);
/* the controller walks the tiles as a grid: Play, Game disc, Player data /
   Video, Audio, Controls / startup, Exit */
static void pad_navigate(launcher_window *s,int key)
{
    static const int grid[3][3]={{0,1,5},{2,3,4},{6,7,7}};
    int row=0,col=0,next,at=s->hover>=0?s->hover:s->selection;
    for(int y=0;y<3;y++)for(int x=0;x<3;x++)if(grid[y][x]==at && !(y==2 && x==2)){row=y;col=x;}
    if(key==VK_RIGHT)next=row==2?(col?6:7):grid[row][(col+1)%3];
    else if(key==VK_LEFT)next=row==2?(col?6:7):grid[row][(col+2)%3];
    else if(key==VK_DOWN)next=row==2?grid[0][0]:grid[row+1][row==1?(col==2?1:0):col];
    else next=row==0?(col==2?7:6):grid[row-1][row==2?(col?2:0):col];
    s->pad_mode=1;hover(s,next);
}
/* the buttons drawn off screen and put up whole: the backdrop's photographs
   repaint them every few frames, half-drawn tiles flickered */
static void draw_button(launcher_window *s,DRAWITEMSTRUCT *d)
{
    if(d->CtlType!=ODT_BUTTON){draw_button_direct(s,d);return;}
    int w=d->rcItem.right-d->rcItem.left,h=d->rcItem.bottom-d->rcItem.top;
    HDC mem=CreateCompatibleDC(d->hDC);HBITMAP bmp=CreateCompatibleBitmap(d->hDC,w,h),old=SelectObject(mem,bmp);
    DRAWITEMSTRUCT off=*d;off.hDC=mem;off.rcItem=(RECT){0,0,w,h};
    draw_button_direct(s,&off);
    BitBlt(d->hDC,d->rcItem.left,d->rcItem.top,w,h,mem,0,0,SRCCOPY);
    SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);
}
static void hover(launcher_window *s,int item)
{
    if(item==s->hover)return;
    int before=s->hover;s->hover=item;
    if(item>=0)s->selection=item;
    if(before>=0)InvalidateRect(s->rows[before],NULL,FALSE);
    if(item>=0)InvalidateRect(s->rows[item],NULL,FALSE);
    InvalidateRect(s->window,NULL,FALSE);
}
static LRESULT CALLBACK window_proc(HWND win,UINT msg,WPARAM w,LPARAM l)
{
    launcher_window *s=(launcher_window *)GetWindowLongPtrW(win,GWLP_USERDATA);
    if(msg==WM_NCCREATE){s=((CREATESTRUCTW *)l)->lpCreateParams;s->window=win;SetWindowLongPtrW(win,GWLP_USERDATA,(LONG_PTR)s);}
    if(!s)return DefWindowProcW(win,msg,w,l);
    switch(msg){
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(win,&ps);HDC back=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,900,700),old=SelectObject(back,bmp);
        draw(s,back);BitBlt(dc,0,0,900,700,back,0,0,SRCCOPY);SelectObject(back,old);DeleteObject(bmp);DeleteDC(back);EndPaint(win,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_MEASUREITEM:((MEASUREITEMSTRUCT *)l)->itemHeight=22;return TRUE;
    case WM_DRAWITEM:draw_button(s,(DRAWITEMSTRUCT *)l);return TRUE;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)w,RGB(0,0,0));SetBkMode((HDC)w,TRANSPARENT);return (LRESULT)GetStockObject(NULL_BRUSH);
    case WM_COMMAND:{
        int id=LOWORD(w),event=HIWORD(w);
        if(id>=ROW && id<ROW+8){
            if(event==BN_SETFOCUS){s->selection=id-ROW;InvalidateRect(win,NULL,FALSE);}
            if(event==BN_CLICKED)activate(s,id-ROW);
        }else if(id==BACK && event==BN_CLICKED)page(s,HOME);
        else if(id==APPLY && event==BN_CLICKED)apply(s);
        else if(id==DEFAULTS && event==BN_CLICKED){
            launcher_options saved=s->current;launcher_options_defaults(&s->current);int p=s->page;page(s,p);s->current=saved;
        }else if(id==TEST_AUDIO && event==BN_CLICKED)test_audio(s);
        else if(id==PORT && event==CBN_SELCHANGE){s->port=(int)SendMessageW(s->port_choice,CB_GETCURSEL,0,0);page(s,CONTROLS);}
        return 0;}
    case WM_SETCURSOR:
        /* P: the launcher follows the mouse; the arrows belong to the game */
        if(s->page==HOME){
            int item=-1;
            for(int k=0;k<8;k++)if((HWND)w==s->rows[k])item=k;
            if(HIWORD(l)==WM_MOUSEMOVE && s->pad_mode && item!=s->hover){s->pad_mode=0;if(s->hover>=0)InvalidateRect(s->rows[s->hover],NULL,FALSE);}
            if(!s->pad_mode)hover(s,item);
        }
        break;
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){if(s->page!=HOME)page(s,HOME);else DestroyWindow(win);}
        if(w==VK_RETURN && s->page==HOME)activate(s,s->selection);return 0;
    case WM_TIMER:{
        ULONGLONG tick=GetTickCount64();double elapsed=(tick-s->last_tick)/1000.;s->last_tick=tick;
        if(s->sdl_ready)controller_tick(s);
        launcher_photos_step(&s->photos,elapsed);
        if(s->photos.current && s->photos.phase>=6.5)RedrawWindow(win,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
        if(s->photos.current && s->photos.phase<elapsed)RedrawWindow(win,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
        return 0;}
    case WM_CLOSE:DestroyWindow(win);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }return DefWindowProcW(win,msg,w,l);
}
static int screenshot(HWND window,const char *path)
{
    HDC dc=GetDC(window),mem=CreateCompatibleDC(dc);BITMAPINFO info={0};info.bmiHeader.biSize=sizeof info.bmiHeader;
    info.bmiHeader.biWidth=900;info.bmiHeader.biHeight=-700;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    uint8_t *pixels;HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);
    if(!bmp){DeleteDC(mem);ReleaseDC(window,dc);return 0;}HBITMAP old=SelectObject(mem,bmp);
    int ok=PrintWindow(window,mem,PW_CLIENTONLY)!=0;
    for(int i=0;i<900*700;i++){uint8_t b=pixels[i*4];pixels[i*4]=pixels[i*4+2];pixels[i*4+2]=b;pixels[i*4+3]=255;}
    size_t bytes=0;void *png=ok?tdefl_write_image_to_png_file_in_memory(pixels,900,700,4,&bytes):NULL;
    wchar_t name[LAUNCHER_PATH];FILE *f=png && wide(path,name,LAUNCHER_PATH)?_wfopen(name,L"wb"):NULL;
    ok=f && fwrite(png,1,bytes,f)==bytes;if(f && fclose(f))ok=0;mz_free(png);
    SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);ReleaseDC(window,dc);return ok;
}
static int run_screen(launcher_startup *start,const char *shot,const char *inputs)
{
    launcher_window s={0};s.start=start;s.hover=-1;s.selection=start->preferences.disc[0]?0:1;
    launcher_options_load(&s.current,start->video_file,start->preferences.saves);
    s.status[0]=0;
    strcpy(s.device,"No controller connected.");HRESULT com=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    s.panel=CreateSolidBrush(RGB(29,32,35));
    s.normal=CreateFontW(-18,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Arial");
    s.title=CreateFontW(-46,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Arial");
    s.font_small=CreateFontW(-15,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Arial");
    s.section=CreateFontW(-26,0,0,0,FW_HEAVY,TRUE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Arial");
    WNDCLASSW cls={0};cls.lpfnWndProc=window_proc;cls.hInstance=GetModuleHandleW(NULL);cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hIcon=LoadIconW(cls.hInstance,MAKEINTRESOURCEW(1));cls.lpszClassName=L"OpenGT4Preparation";
    if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){s.result=-1;goto end;}
    RECT r={0,0,900,700};AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);
    HWND win=CreateWindowW(cls.lpszClassName,L"OpenGT4",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,NULL,NULL,cls.hInstance,&s);
    if(!win){s.result=-1;goto end;}
    /* P: a larger start object and a separate row of settings objects, like
       the pre-race toolbar, leave the photograph visible. No drawn cursor. */
    char icon_folder[LAUNCHER_PATH+32],base[LAUNCHER_PATH];
    strcpy(base,s.start->preferences.file);char *slash=strrchr(base,'/'),*backslash=strrchr(base,'\\');
    if(backslash && (!slash || backslash>slash))slash=backslash;
    if(slash)*slash=0;else strcpy(base,".");
    snprintf(icon_folder,sizeof icon_folder,"%s/launcher_icons",base);
    {   /* the icons beside the configuration, else beside the executable
           (a release ships them in its own folder) */
        char probe[LAUNCHER_PATH+64];snprintf(probe,sizeof probe,"%s/%s",icon_folder,"places-folder.png");
        FILE *f=fopen(probe,"rb");
        if(f)fclose(f);
        else{
            wchar_t exe[LAUNCHER_PATH];DWORD n=GetModuleFileNameW(NULL,exe,LAUNCHER_PATH);
            if(n && n<LAUNCHER_PATH){
                wchar_t *cut=wcsrchr(exe,L'\\');if(cut)*cut=0;
                char dir[LAUNCHER_PATH];
                if(WideCharToMultiByte(CP_UTF8,0,exe,-1,dir,LAUNCHER_PATH,NULL,NULL))
                    snprintf(icon_folder,sizeof icon_folder,"%s/launcher_icons",dir);
            }
        }
    }
    s.icons=launcher_icons_create(icon_folder,80);s.large_icons=launcher_icons_create(icon_folder,144);
    /* the tiles under their sections: Game (Play, Game disc, Player data),
       Settings (Video, Audio, Controls); the startup switch and the exit */
    const int xy[8][4]={{46,158,154,154},{212,158,154,154},{46,376,154,154},{212,376,154,154},
        {378,376,154,154},{378,158,154,154},{40,592,260,26},{784,588,72,44}};
    for(int i=0;i<8;i++)s.rows[i]=control(&s,L"BUTTON",row_names[i],BS_OWNERDRAW|BS_NOTIFY|WS_TABSTOP,
        ROW+i,xy[i][0],xy[i][1],xy[i][2],xy[i][3]);
    load_photos(&s);
    page(&s,HOME);ShowWindow(win,SW_SHOW);UpdateWindow(win);
    s.sdl_ready=SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)==0;
    if(s.sdl_ready)controller_tick(&s);
    s.last_tick=GetTickCount64();SetTimer(win,1,33,NULL);
    /* Reproducible UI probes use the real window messages and fields; no ISO
       is required. Play is opt-in; probes never open path selectors. */
    char *sequence=inputs?strdup(inputs):NULL,*token=sequence;
    for(char *next;token && *token;token=next){
        next=strchr(token,',');if(next)*next++=0;
        if(!strcmp(token,"video"))SendMessageW(win,WM_COMMAND,ROW+2,0);
        else if(!strcmp(token,"audio"))SendMessageW(win,WM_COMMAND,ROW+3,0);
        else if(!strcmp(token,"controls"))SendMessageW(win,WM_COMMAND,ROW+4,0);
        else if(!strcmp(token,"left") || !strcmp(token,"right") || !strcmp(token,"up") || !strcmp(token,"down")){
            /* the controller's directions (the keyboard's arrows do not walk the tiles) */
            int key=!strcmp(token,"left")?VK_LEFT:!strcmp(token,"right")?VK_RIGHT:!strcmp(token,"up")?VK_UP:VK_DOWN;
            if(s.page==HOME)pad_navigate(&s,key);
        }
        else if(!strcmp(token,"back"))SendMessageW(win,WM_COMMAND,BACK,0);
        else if(!strcmp(token,"apply"))SendMessageW(win,WM_COMMAND,APPLY,0);
        else if(!strcmp(token,"default"))SendMessageW(win,WM_COMMAND,DEFAULTS,0);
        else if(!strncmp(token,"focus:",6)){int selected=atoi(token+6);if(s.page==HOME && selected>=0 && selected<8){SetFocus(s.rows[selected]);hover(&s,selected);}}
        else if(!strncmp(token,"hover:",6)){int item=atoi(token+6);if(s.page==HOME && item>=0 && item<8)SendMessageW(win,WM_SETCURSOR,(WPARAM)s.rows[item],MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));}
        else if(!strcmp(token,"startup"))SendMessageW(win,WM_COMMAND,ROW+6,0);
        else if(!strcmp(token,"play"))SendMessageW(win,WM_COMMAND,ROW,0);
        else if(!strcmp(token,"test-audio") && s.page==AUDIO)SendMessageW(win,WM_COMMAND,TEST_AUDIO,0);
        else if(!strcmp(token,"2p")){SendMessageW(s.port_choice,CB_SETCURSEL,1,0);SendMessageW(win,WM_COMMAND,MAKEWPARAM(PORT,CBN_SELCHANGE),0);}
        else if(!strncmp(token,"wait:",5))launcher_photos_step(&s.photos,strtod(token+5,NULL));
        else if(!strncmp(token,"set:",4)){int field,choice;if(sscanf(token+4,"%d:%d",&field,&choice)==2 && field>=0 && field<s.count)SendMessageW(s.fields[field],CB_SETCURSEL,choice,0);}
    }free(sequence);UpdateWindow(win);if(inputs)printf("LAUNCHER FOCUS %d\n",s.selection);
    if(shot){s.result=screenshot(win,shot)?0:-1;DestroyWindow(win);}
    MSG msg;while(GetMessageW(&msg,NULL,0,0)>0){
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE){
            HWND f=GetFocus();wchar_t name[32];GetClassNameW(f,name,32);
            if(!wcscmp(name,L"ComboBox") && SendMessageW(f,CB_GETDROPPEDSTATE,0,0))SendMessageW(f,CB_SHOWDROPDOWN,FALSE,0);
            else SendMessageW(win,WM_KEYDOWN,VK_ESCAPE,0);continue;
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN){
            HWND focus=GetFocus();wchar_t name[32];GetClassNameW(focus,name,32);
            if(!wcscmp(name,L"Button")){SendMessageW(focus,BM_CLICK,0,0);continue;}
            if(!wcscmp(name,L"ComboBox")){SendMessageW(focus,CB_SHOWDROPDOWN,!SendMessageW(focus,CB_GETDROPPEDSTATE,0,0),0);continue;}
        }
        if(msg.message==WM_KEYDOWN && s.page==HOME && (msg.wParam==VK_LEFT || msg.wParam==VK_RIGHT || msg.wParam==VK_UP || msg.wParam==VK_DOWN))
            continue;   /* no keyboard walk through the tiles: the mouse chooses */
        if(!IsDialogMessageW(win,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
end:
    for(int p=0;p<2;p++)if(s.pads[p])SDL_GameControllerClose(s.pads[p]);
    if(s.audio)SDL_CloseAudioDevice(s.audio);
    SDL_QuitSubSystem(SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS);
    launcher_photos_close(&s.photos);
    if(s.backdrop)DeleteObject(s.backdrop);if(s.icons)DeleteObject(s.icons);if(s.large_icons)DeleteObject(s.large_icons);
    DeleteObject(s.normal);DeleteObject(s.font_small);DeleteObject(s.title);DeleteObject(s.section);DeleteObject(s.panel);
    if(SUCCEEDED(com))CoUninitialize();return s.result;
}
#endif
int launcher_bootstrap(int *argc,char ***argv,launcher_startup *start)
{
    if(*argc>1 && strcmp((*argv)[1],"--launcher"))return 1;
    int force=*argc>1;const char *file=NULL,*disc=NULL,*shot=NULL,*inputs=NULL;
    for(int i=2;i<*argc;i++){
        const char *arg=(*argv)[i];
        if(!strcmp(arg,"--launcher-config") && i+1<*argc)file=(*argv)[++i];
        else if(!strcmp(arg,"--shot") && i+1<*argc)shot=(*argv)[++i];
        else if(!strcmp(arg,"--input") && i+1<*argc)inputs=(*argv)[++i];
        else if(arg[0]!='-' && !disc)disc=arg;else return -1;
    }
    if(file && strlen(file)>=LAUNCHER_PATH-32)return -1;
    char *pref=SDL_GetPrefPath("OpenGT4","OpenGT4");if(!pref)return -1;
    char config[LAUNCHER_PATH];snprintf(config,sizeof config,"%slauncher.ini",pref);
    launcher_preferences_load(&start->preferences,file?file:config);
    snprintf(start->video_file,sizeof start->video_file,file?"%s.video.ini":"%svideo.ini",file?file:pref);
    if(!start->preferences.saves[0])snprintf(start->preferences.saves,sizeof start->preferences.saves,"%ssaves",pref);
    SDL_free(pref);
    if(disc){if(strlen(disc)>=LAUNCHER_PATH || strpbrk(disc,"\r\n"))return -1;strcpy(start->preferences.disc,disc);}
    char message[256];
    /* On normal startup the UI needs no disc access. Only bypass/Play and the
       file selector validate metadata. Art/audio/scripts are never opened. */
    if(force || start->preferences.show || !launcher_validate_disc(start->preferences.disc,message,sizeof message)){
#ifdef _WIN32
        int result=run_screen(start,shot,inputs);if(result!=1)return result;
#else
        fprintf(stderr,"Standalone preparation currently requires Windows. Pass the ISO on the command line.\n");return -1;
#endif
    }
    start->arguments[0]=(*argv)[0];start->arguments[1]=start->preferences.disc;
    start->arguments[2]="--save-dir";start->arguments[3]=start->preferences.saves;
    start->arguments[4]="--config-file";start->arguments[5]=start->video_file;start->arguments[6]=NULL;
    *argc=6;*argv=start->arguments;return 1;
}
