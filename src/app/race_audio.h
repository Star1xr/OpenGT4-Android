#ifndef OPENGT4_APP_RACE_AUDIO_H
#define OPENGT4_APP_RACE_AUDIO_H
#include "race_run.h"
#include "course.h"
#include "race_pit_crew.h"
typedef struct race_audio race_audio;
race_audio *race_audio_open(const char *disc,const gt4_race_run *run,const gt4_course *course);
/* 0033C180/0033C278: replay has an independent playlist by default. */
race_audio *race_audio_open_replay(const char *disc,const gt4_race_run *run,const gt4_course *course,int replay);
/* Each drawn frame: the camera the race is seen from (view 0 bumper,
   1 behind, 6 roof; -1 the start's or the free camera), the volumes. */
void race_audio_update(race_audio *audio,const gt4_race_run *run,
                       const float eye[3],const float rows[9],int view,int bgm,int se);
/* Each race frame (1/60 s), as the game's sound runs: the cars, the
   course's ambience and the driver, from the last camera given. */
void race_audio_step(race_audio *audio,const gt4_race_run *run);
/* 003B6B78 gates the pulse with the resident model outlet count. */
void race_audio_set_exhaust_count(race_audio *audio,unsigned entry,unsigned count);
/* The backfire's four flames of car `entry` (time, size, turn); 1 when one burns */
int race_audio_afterfire(const race_audio *audio,unsigned entry,float out[4][3]);
/* 003CFA98/003D3358: watched-car shots and one shared refueling loop. */
void race_audio_pit(race_audio *audio,const gt4_pit_crew_sound *sound);
/* A menu's play_sound over the race (pause.strb, result.strb): "ok",
   "cursor", "cancel", at the SE option `se` (0..128). */
void race_audio_menu_sound(race_audio *audio,const char *name,int se);
/* While the race is held: the menu's sounds and the driver go on. */
void race_audio_hold(race_audio *audio,double dt);
/* the start's sound (count_s), with START */
void race_audio_start(race_audio *audio);
/* The pause opened (1) or closed (0): the cars' and the course's
   sounds stopped, then started again. */
void race_audio_pause(race_audio *audio,int on);
void race_audio_close(race_audio *audio);
/* once a frame: the time to the music's next mark (NaN: none), *passed
   when one went by or the stream changed (0033D170) */
float race_audio_music_cue(race_audio *audio, int *passed);
/* the playing track's title and artist, 0 when none */
int race_audio_music_title(race_audio *audio, char *title, size_t title_size, char *artist, size_t artist_size);
#endif
