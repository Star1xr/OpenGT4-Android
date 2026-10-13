/* SDL-specific extras the app uses until input moves behind platform.h. */
#ifndef OPENGT4_PLAT_SDL_H
#define OPENGT4_PLAT_SDL_H

typedef struct {
    int key; char text[32];    /* SDL keycode */
    int pad;                  /* controller event; race owns its edges */
    /* a PS2 button the menus' scripts take as its own key (the engine's
       D_006192E4 keysyms: L1 0xFFC8, L2 0xFFC9, R1 0xFFD2, R2 0xFFD3,
       Square 0xFF63, Select 0xFFFF), pressed or released; key 0 then */
    int ps2key, release;
    int mouse, mouse_x, mouse_y, wheel; /* PC camera events, never PS2 pad keys */
} plat_key_event;

/* Handles window events; returns 1 and fills ev for each key press. */
int plat_sdl_poll(plat_key_event *ev);

/* Deterministic command-line UI tests only; -1 restores interactive dialogs. */
void plat_sdl_dialog_answer(int answer);
int plat_sdl_get_dialog_answer(void);
unsigned plat_sdl_audio_underruns(void);

/* Video (video_gl.c). */
#include <SDL.h>
SDL_GameController *plat_sdl_pad(int index); /* borrowed until disconnect/shutdown */
/* the PS2 keysym an event stands for (the controller's L1/L2/R1/R2,
   Square and Select; the keyboard's Q/E held as L1/R1, Insert and
   Delete), 0 none; *release for its letting go */
int plat_sdl_ps2_key(const SDL_Event *e, int *release);
unsigned plat_sdl_menu_buttons(void);
int plat_sdl_controller_key(const SDL_Event *event);
SDL_Window *plat_sdl_window(void);
/* RGBA pixels of the current frame (before present), top row first; the
   caller frees them. Size of the window's drawable. */
unsigned char *plat_sdl_read_pixels(int *width, int *height);
unsigned char *plat_sdl_read_view(int *width, int *height);   /* the game's area only, without the window's bars */
#endif
