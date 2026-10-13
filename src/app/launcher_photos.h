#ifndef OPENGT4_LAUNCHER_PHOTOS_H
#define OPENGT4_LAUNCHER_PHOTOS_H
#include <stdint.h>
/* P: local native photographs. Never read the disc or bundle game imagery. */
typedef struct {
    char paths[32][1100];
    int count,index,width,height;
    uint32_t *current,*next;
    double phase;
} launcher_photos;
int launcher_photos_open(launcher_photos *p,const char *cache,const char *album,int width,int height);
void launcher_photos_close(launcher_photos *p);
void launcher_photos_step(launcher_photos *p,double seconds);
/* Writes BGRX pixels for a Windows DIB; zero means no available photograph. */
int launcher_photos_draw(const launcher_photos *p,uint32_t *pixels);
#endif
