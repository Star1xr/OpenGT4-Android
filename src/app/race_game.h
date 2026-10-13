/* A race in the game: the native race (src/core/physics/race_run.c) with
   the player's controller, drawn by the race view. */
#ifndef OPENGT4_RACE_GAME_H
#define OPENGT4_RACE_GAME_H

#include <stddef.h>
#include "physics/race_run.h"
#include "menu.h"
#include "replay_file.h"
#include "logger.h"

typedef struct race_game race_game;

race_game *race_game_open(const char *disc);
void race_game_close(race_game *g);

/* A race on `course` with the player's car (GENERIC_CAR label, paint) and
   the other cars (labels and paints, colors may be NULL; up to 5). The physics is set up at once; the
   models load on a thread (race_game_poll). 0 with msg on failure. */
int race_game_start(race_game *g, const char *course, const char *car, int color,
                    const char *const *others, const int *colors, int nothers, char *msg, size_t len);
/* Prepared GT grid: consumes the descriptor synchronously, retains owned
   buffers for restart/replay, and draws paints from the constructed setups. */
int race_game_start_prepared(race_game *g,const gt4_race_desc *desc,char *msg,size_t len);
int race_game_license_active(const race_game *g);
/* the race is a driving mission (RaceMission, desc.license 3) */
int race_game_mission_active(const race_game *g);
int race_game_license_result(const race_game *g,int32_t *state,uint32_t *time);
int race_game_license_retry(const race_game *g);
/* Borrowed until the next start/close; player telemetry, no invented ghost. */
const gt4_logger_sampler *race_game_license_log(const race_game *g);
/* the Logger's tracks from the last race (Time Trial, licences): entries
   0 and 1 as they ran; 0 when there is nothing to show */
int race_game_logger_source(const race_game *g,menu_logger_source *s);
/* the Logger's Load Track: the session replayed with no picture, the
   loaded best lap (the ghost) as the track asked for (0 A, 1 B) */
int race_game_logger_load(race_game *,const gt4_replay_file *,int track,char *msg,size_t len);
int race_game_logger_analyze(race_game *g,int track,char *msg,size_t len);
/* the same, a frame at a time (00122FA8 on its thread): begin, step for
   budget seconds (1 done, 0 more, -1 failed; progress 0..1), end (ok 0:
   stopped, the race back as it was before the analysis) */
int race_game_logger_analyze_begin(race_game *g,char *msg,size_t len);
int race_game_logger_analyze_step(race_game *g,double budget,float *progress);
void race_game_logger_analyze_end(race_game *g,int track,int ok);
/* the last race to a replay file (its tape lent) and a file's race back as
   the last one, for race_game_replay */
int race_game_replay_export(const race_game *g, gt4_replay_file *r, int kind);
/* The Time Trial's ghost kept for the session (its best lap, carried into
   the next start on the same course with the same car): whether there is
   one, it as a best-lap replay (lent), and a saved one taken in its place
   (0 when it was driven elsewhere or with another car) */
int race_game_has_ghost(const race_game *g);
int race_game_ghost_export(race_game *g, gt4_replay_file *r);
int race_game_ghost_import(race_game *g, const gt4_replay_file *r);
/* the saved replay's picture: wanted once the player has finished, given
   the drawn race frame (RGBA) */
int race_game_wants_thumbnail(const race_game *g);
void race_game_set_thumbnail(race_game *g, const uint8_t *rgba, int w, int h);
int race_game_replay_import(race_game *g, const gt4_replay_file *r);
/* Disc demonstrations use the same native pad playback as saved replays. */
int race_game_read_replay(race_game *g,const char *name,gt4_replay_file *r,char *msg,size_t len);
/* NULL code selects the current examination's owned label. */
int race_game_license_demo(race_game *g,const char *code,char *msg,size_t len);
int race_game_demonstrating(const race_game *g);
int race_game_end_demo(race_game *g,char *msg,size_t len);
int race_game_restart(race_game *g,char *msg,size_t len);
/* Refresh the saved player setup before restarting a prepared GT event.
   Does not mutate a running simulation or the physics implementation. */
int race_game_setting(race_game *g,const uint8_t body[0x4b8]);
/* 0 loading, 1 ready, -1 failed (the race is dropped) */
int race_game_poll(race_game *g);
/* dt seconds of play: the controller read, the physics at 60 steps a
   second, the cars placed for drawing */
void race_game_update(race_game *g, double dt);
int race_game_draw(race_game *g);
void race_game_pregrid_update(race_game *g,double dt);
int race_game_draw_pregrid(race_game *g);
/* MenuRaceBase's end/restart fade, after drawing all menu pages. */
void race_game_draw_pregrid_overlay(race_game *g);
/* Retain the course behind the returned menu; starting again resets physics. */
void race_game_return_pregrid(race_game *g);
/* the race display over the drawn view (after the 3D scene) */
void race_game_draw_hud(race_game *g, double dt);
/* the speed's unit on the display: 0 km/h, 1 mph */
void race_game_set_unit(race_game *g, int unit);
/* the power's unit on the replay's opening: the unit_power option's text */
void race_game_set_power_unit(race_game *g, const char *unit);
/* a key for the pause menu (Escape holds/continues, arrows and Enter in it) */
void race_game_key(race_game *g, int key);
/* 1 once the pause menu chose to leave the race */
int race_game_quit(const race_game *g);
void race_game_set_audio(race_game *g, int bgm, int enabled, int se);
/* User-requested PC inspection feature (P): F3 toggles an unclipped camera. */
void race_game_inspect_toggle(race_game *g);
/* P: manual PC filming, F9. Free/follow/body mount, target focus and lens.
   Camera state is presentation only; controller driving stays live. */
int race_game_cinematic(race_game *g);
void race_game_cinematic_toggle(race_game *g);
int race_game_cinematic_key(race_game *g,int key);
void race_game_cinematic_mouse(race_game *g,int dx,int dy,int wheel);
/* the race dropped (back to the menus) */
void race_game_stop(race_game *g);
int race_game_active(const race_game *g);
/* the GENERIC_CAR id of entry i of the race set up (0 the player's), 0
   when there is none */
unsigned race_game_car_id(const race_game *g, int i);
/* seconds since the player's car finished, -1 before */
double race_game_finished(const race_game *g);
/* 1 once the results let the race go (their movie's cb_raceexit) */
int race_game_over(const race_game *g);
/* The replay: the last race's commands for the player's car, one a
   step. can_replay: there is one; race_game_replay starts that race again
   (as race_game_start, loading) playing them back, through the TV
   cameras; replaying: this race is one. */
int race_game_can_replay(const race_game *g);
int race_game_replay(race_game *g, char *msg, size_t len);
int race_game_replaying(const race_game *g);
/* the replay's first mode from the options: replay_display_enable, replay_mode DIVE */
void race_game_set_replay_options(race_game *g, int display, int dive);
/* the driving options of the next race_game_start: the tyres (the
   Arcade's index, -1 the car's own), the gearbox and the aids */
void race_game_set_driving(race_game *g, int tire, int manual, int assist_asm, int assist_tcs);
/* The Arcade's QuickTune at Start (00111DC0): 1 when the built race must
   be rebuilt to take it. Cleared for a newly selected race (00111B50). */
int race_game_quick_tune(race_game *g, int tuned, int power, int weight, int front, int rear,
                         int gear, int transmission, int assist);
void race_game_quick_clear(race_game *g);
/* the player's gearbox range for the QuickTune (00111B50) */
int race_game_gear_range(const race_game *g, int *gear, int *lo, int *hi);
/* the laps of the next race_game_start (0: the default two) */
void race_game_set_laps(race_game *g, int laps);
/* Power & Speed: car 0 through (1), its test (the laps word: 1 400 m, 2
   1000 m, 0/3 top speed), best time (ms) and top speed (m/s) */
int race_game_machine_result(const race_game *g,int *test,uint32_t *time,float *speed);
/* Two local humans; NULL restores the ordinary single-player start. */
void race_game_set_split(race_game *g,const gt4_race_desc *options);
/* Owned garage selection for Arcade/Time Trial; NULL selects catalog. */
void race_game_set_arcade_owned(race_game *g,const uint8_t *body);
int race_game_split(const race_game *g);
int race_game_take_retry(race_game *g);
/* the next races are the Arcade's Time Trial (00112620) */
void race_game_set_time_trial(race_game *g, int on);
/* the next race's world mode (GT Mode Free Run 11, Practice 12; 0 the path's own) */
void race_game_set_world_mode(race_game *g, int mode);
/* B-Spec (the quick menu's Spectator): the player's car driven by its
   driver, the pad's orders (pace, overtake, pit) sent through the race's
   event block. Returns 1 when the built race must be rebuilt. */
int race_game_set_bspec(race_game *g, int on);
int race_game_bspec(const race_game *g);
/* 003C12A8: race options +970, A/B-Spec takeover allowed at the box. */
void race_game_set_pit_mode_change(race_game *g,int allowed);
/* the player's best lap of the race just run (0x157529FF none) */
uint32_t race_game_best_lap(const race_game *g);
int race_game_player_wear(const race_game *g,uint8_t wear[32]);
int race_game_player_distances(const race_game *g,uint32_t *a,uint32_t *b);
/* the race's driving mode, world+0xCC8 (0 A-Spec, 1 B-Spec): 00114498 /
   00119D38 score A-Spec points only for 0 or 2 */
int race_game_driver_mode(const race_game *g);
int race_game_player_hud(const race_game *g,gt4_race_hud *hud);
int race_game_championship_grid(const race_game *g,gt4_race_desc *desc,uint8_t bodies[6][0x4b8]);
int race_game_finish_order(const race_game *g,uint8_t order[6]);
int race_game_quick_grid(const race_game *g,menu_quick_race *grid);
int race_game_quick_results(const race_game *g,menu_quick_race *grid);
int race_game_event_position(const race_game *g);

struct race_view;
struct race_view *race_game_photo_view(race_game *);
void race_game_photo_pause(race_game *,int on);
int race_game_photo_camera_data(race_game *,float size[4],float *distance);
#endif
