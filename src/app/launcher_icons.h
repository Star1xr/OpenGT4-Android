#ifndef OPENGT4_LAUNCHER_ICONS_H
#define OPENGT4_LAUNCHER_ICONS_H
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
/* P: separately downloaded public-domain Tango PNGs. Missing/corrupt files
   leave their slots transparent; captions/navigation remain available. */
HBITMAP launcher_icons_create(const char *folder,int size);
void launcher_icon_draw(HDC dc,HBITMAP atlas,int icon,int x,int y,int size);
#endif
#endif
