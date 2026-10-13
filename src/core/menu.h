/*
 * Menu runtime: one UI project (.mproject screens) together with its Adhoc
 * scripts, run natively. The scripts see the screens' widgets as modules and
 * the engine through main::game, main::sound, main::menu, ... which this
 * layer provides. Engine parts not written yet answer as stubs and are
 * reported once in the log (see docs/ADHOC.md).
 */
#ifndef OPENGT4_MENU_H
#define OPENGT4_MENU_H

#include <stdint.h>
#include "career_license.h"

#include "ui.h"
#include "vol.h"
#include "ps2_media.h"
#include "adhoc/adhoc_vm.h"
#include "replay_file.h"
#include "photo_mode.h"
#include "pad_config.h"
#include "logger_view.h"

typedef struct menu menu;
int menu_import_ps2_media(menu *m,const char *path,gt4_media_import *report);
/* photo_shoot sequence boundary, selected scene and owned garage car. */
int menu_photo_request(menu *m,char course[32],char car[96],int *color);
/* its car's fitted wheel and wing labels (the garage's car), empty stock */
int menu_photo_parts(menu *m,char wheel[32],char wing[32]);
gt4_photo_mode *menu_photo_state(menu *m);
void menu_photo_set_location(menu *m,const gt4_course *course);
void menu_photo_set_selection(menu *m,const char *course,const char *car,int color);

menu       *menu_open(gt4_vol *vol, const char *project, char *err, size_t err_len);
/* Loading pump runs on the caller thread and must not enter menu scripts. */
menu *menu_open_presented(gt4_vol *vol,const char *project,char *err,size_t err_len,void (*progress)(void *),void *user);
void        menu_close(menu *m);
ui_project *menu_ui(menu *m);
/* Port options can extend authored widgets through the same event bridge. */
void menu_native_method(menu *m,ui_widget *w,const char *method,ah_native_fn fn,void *user);
void menu_native_page(menu *m,int root);
void menu_native_focus(menu *m,ui_widget *w);
void menu_play_effect(menu *m,const char *name);
void menu_request_project(menu *m,const char *name);
void menu_save_refresh(menu *m);
void menu_save_capture(menu *m);
int menu_setting_begin(menu *m);
void menu_setting_context(menu *m,menu *source);
int menu_setting_end(menu *m);
int menu_riding_car_body(menu *m,uint8_t body[0x4b8]);
/* GT Mode's Free Run (1) / Practice (2) / Photo Drive (3) asked (next_sequence), 0 none */
int menu_race_is_free(menu *m);
/* their course (game.course_code) and the garage's riding car */
int menu_free_request(menu *m,char course[32],char car[96],int *color,uint8_t body[0x4b8],int *laps);
int menu_photo_drive_rules(menu *m,uint8_t row[151]);
int menu_take_gtmode_save_entry(menu *m);
int menu_confirm(menu *m,const char *title,const char *message);
void menu_options_storage(const char *path);
double menu_option_number(menu *m,const char *name,double fallback);
int menu_driving_manual(menu *m);
void menu_pad_configuration(menu *m,int port,gt4_pad_config *out);
const char *menu_option_text(menu *m,const char *name,const char *fallback); /* a string option, or fallback */
const char *menu_game_text(menu *m,const char *name); /* main.game.<name> as a string, "" without one */

/* Opens screen i directly (debug browsing). Initialize runs once per opening;
   closing/finalizing allows a later opening to initialize again. */
void menu_enter(menu *m, int root);
/* One frame (1/60 s): script watchers and garbage collection. */
void menu_tick(menu *m);
void menu_advance(menu *m, double seconds);
typedef enum { MENU_UP, MENU_DOWN, MENU_LEFT, MENU_RIGHT, MENU_ACCEPT, MENU_BACK, MENU_TRIANGLE, MENU_CIRCLE, MENU_SQUARE } menu_input;
void menu_set_photo_saver(menu *m,int (*fn)(void *),void *user);
void menu_set_profile_picker(menu *m,int (*fn)(menu *));
void menu_set_dialog_presenter(menu *m,int (*fn)(menu *,const char *,const char *,int,const char *,const char *));
/* the common ProgressDialog: state 1 opens it (message, an OK button),
   0 closes it, 2 sets its progress (0..1, -1 unknown) */
void menu_set_progress_presenter(menu *m,void (*fn)(menu *,int state,const char *message,int button,float progress));
void menu_set_presenter(menu *m, void (*fn)(menu *,float,void *), void *user);
/* A modal script loop keeps its caller suspended while the platform pumps
   input and presentation. Return zero only when the application is quitting. */
void menu_set_event_presenter(menu *m, int (*fn)(menu *,void *), void *user);
/* Platform loader for CarFace.syncLoading; nonzero only when the requested model is ready. */
void menu_set_car_loader(menu *m,int (*fn)(menu *,const ui_widget *,void *),void *user);
void menu_animate(menu *m, float seconds);
float menu_cursor_speed(menu *m);
void menu_send_input(menu *m, menu_input input);
/* a PS2 button the scripts take as its own key (keysym, the engine's
   D_006192E4: L1 0xFFC8, L2 0xFFC9, R1 0xFFD2, R2 0xFFD3, Square
   0xFF63, Select 0xFFFF), pressed or released */
void menu_send_key(menu *m, int keysym, int release);
/* Delivers MovieFace.onMovieFinish for the active page, then applies any
   page/project transition requested by the authored script. */
int menu_movie_finished(menu *m);
int menu_active_root(const menu *m);
/* The page stack, bottom (0) to top (depth - 1): pushPage keeps pages under
   the new one, startPage replaces the top. */
int menu_page_depth(const menu *m);
int menu_page_at(const menu *m, int i);
const char *menu_focus_name(const menu *m);
/* Consumes a script request to open another project; empty means none. */
int menu_take_project(menu *m, char *name, size_t capacity);
int menu_take_photo_film(menu *,char *path,size_t capacity);
/* Explicit disc catalog preview; does not load or modify save unlocks. */
void menu_preview_courses(menu *m,int enabled);
const char *menu_selected_course(menu *m);
const char *menu_selected_car(menu *m);
/* The paint CarRoot's script chose (CarFace.setColorIndex), -1 before any. */
int menu_car_color(const menu *m);
/* 1 once after the scripts finished into a race sequence (single race,
   time trial): the caller starts the race. */
int menu_take_race_request(menu *m);
int menu_race_is_time_trial(menu *m);
/* _tmp_gtmode.ad L10918..10928: event identity and selected garage car.
   Snapshot owns its bytes and survives closing/reloading the menu VM.
   Does not start a race or substitute a random seed. */
typedef struct {
    char race[32];
    uint8_t player_body[0x4b8];
    int championship;
} menu_career_request;
int menu_get_career_request(menu *m,menu_career_request *out,char *error,size_t capacity);
int menu_race_is_license(menu *m);
int menu_get_license_request(menu *m,char code[32]);
uint32_t menu_license_best(menu *m,const char *code);
int menu_license_begin(menu *m,const char *code,gt4_career_license_attempts *visit);
int menu_license_attempt(menu *m,const char *code,gt4_career_license_attempts *visit,int32_t state,uint32_t time,uint32_t date);
int menu_license_leave(menu *m,const char *code,const gt4_career_license_attempts *visit);
void menu_license_return_prepare(menu *m);
int menu_apply_car_wear(menu *m,const uint8_t wear[32]);
int menu_apply_event_distances(menu *m,uint32_t a,uint32_t b);
/* Commit once, before quick-menu exit/next/retry can replace the live race. */
int menu_commit_career_distances(menu *career,menu *quick,uint32_t a,uint32_t b,int *pending);
/* gtmode.ad:141..154: re-evaluate authored project prizes on return. */
int menu_enter_career_return(menu *m);
int menu_race_is_mission(menu *m);
void menu_request_mission(menu *m,const char *code);   /* tests */
void menu_test_push(menu *m,const char *names);          /* tests: "tuners,spoon" */
void menu_test_money(menu *m,int64_t amount);             /* tests */
void menu_test_car(menu *m,const char *label);              /* tests */
int menu_get_mission_request(menu *m,char code[32]);
/* 0011C328 / 0011C2E0: a mission's result on the profile (record, prize
   total, calendar); the quick-mission script pays the prize */
int menu_apply_mission_result(menu *m,const char *race,int finished,int32_t position,
    uint32_t race_time,int32_t aspec,uint32_t prize,uint32_t *award);
int menu_apply_career_result(menu *m,const char *race,int finished,int32_t position,
    uint32_t best_lap,int32_t mode,int32_t aspec,uint32_t prize,uint8_t *best,uint32_t *award);
int menu_apply_championship_result(menu *m,const char *race,const char *series,
    int finished,int final_session,int32_t position,uint32_t best_lap,int32_t mode,int32_t aspec,
    uint32_t prize,const int32_t points[6],int16_t units,uint32_t *award,uint32_t *grand);
void menu_quick_event_result(menu *m,int finished,uint32_t prize);
void menu_quick_event_context(menu *m,const char *race);
/* 00110498, also written to race options +970 by 00114250/00119918. */
int menu_career_aspec_available(menu *m,const char *race);
int menu_enter_quick_start(menu *m);
void menu_set_origin(menu *m, const char *project);
/* The cars' ratings the Arcade picks opponents by (gt4_car_ratings_get:
   keys in, 0..1 ratings out, 1 when done). */
void menu_set_car_rater(menu *m, int (*rate)(void *, const uint64_t *, int, float *), void *ctx);
/* The opponents the scripts entered for the race (game.setEntryCarCode
   1..overwrite_enemy_num): labels (valid until the menu changes) and
   paints. Returns how many. */
int menu_race_entries(menu *m, const char **cars, int *colors, int max);

/* The race the quick menus show (MQuickWork, the game's quick work at
   D_006187A8): the course, the grid from pole (GENERIC_CAR labels, paint
   indices, power in PS and weight in kg as the game computes them), the
   player's place, the tyres' kind every car runs. Shared by every
   project's scripts. */
typedef struct {
    char course[32];
    int entries, player;
    struct { char car[32]; int color, power, weight; uint32_t time; } grid[6];
    int tire;
    /* 00111B50: the player's gearbox top speed (setup +0x12A) and its
       TRANSMISSION range (+0x26/+0x27), 0 when not adjustable */
    int gear, gear_min, gear_max;
    int can_replay, dry;
    /* 00119918: quick +0x4f4 laps, +0x4f0 rounded course metres. */
    int32_t laps, course_length;
    /* GT event work: 00114250 +0x46c/+0x490/+0x4a4 and the authored
       event/championship getStartPage selectors. Supplied by the caller;
       entering a menu must never manufacture a completed result. */
    char race[32];
    int finished, bspec;
    uint32_t prize, grand_prize;
    int championship,session_number,final_session,series_rank;
    int32_t total_points[6],round_points[6];
    int license,license_load_ghost,license_disable_logger;
    uint32_t gold_time,silver_time,bronze_time;
} menu_quick_race;
void menu_set_quick_race(const menu_quick_race *q);
/* The Logger's two tracks (logger.ad, MLoggerControl): the last race's
   samples of its entries 0 and 1 (00122FA8), track B NULL when there is
   none; the samples stay the caller's while the Logger shows them. NULL
   forgets them. */
typedef struct {
    const gt4_logger_sampler *track[2];
    uint32_t time[2];      /* each entry's laps added up (00122D38), ms */
    char car[2][32];       /* car codes */
    int power[2];          /* PS */
    char course[32];       /* course code (CourseName) */
    char mode[32];         /* RaceModeName key */
    int subnumber;         /* race_mode_subnumber, -1 none */
} menu_logger_source;
void menu_set_logger_source(const menu_logger_source *s);
/* the Logger's analysis by the race's owner, a step a frame: begin (1
   started), step (1 done, 0 more, -1 failed; progress 0..1), end (ok;
   it sets the new source with menu_set_logger_source when ok) */
void menu_set_logger_analyzer(void *user,int (*begin)(void *),int (*step)(void *,float *),void (*end)(void *,int));
/* the pad's sticks for the Logger's graph (001206A0): left x, y, right x, y */
void menu_set_sticks(menu *m,const float axes[4]);
/* the Logger left asking for a best lap as track 0 (A) or 1 (B); -1 none */
int menu_take_logger_load(void);
/* sound.pauseStream: the PCM stream held */
int menu_pcm_paused(menu *m);
/* the slide project (Jukebox) given its page and way back */
void menu_jukebox_context(menu *m,menu *source);
/* The Arcade's QuickTune at Start (quick work +0x3AC..+0x3C4, +0x488),
   IsQuickTuned consumed as 00111DC0 does */
typedef struct {int tuned,power,weight,front_tire,rear_tire,gear,transmission,assist;} menu_quick_tune_values;
int menu_quick_tune(menu *m,menu_quick_tune_values *out);
/* Replace rebuilt entry statistics while retaining event/result metadata. */
void menu_update_quick_player(int32_t power,int32_t weight);
void menu_set_quick_completed(menu *m,int completed);
/* the cached Arcade back from the race's quick menu: what its reload runs */
void menu_arcade_return(menu *m);
/* the session's best time (MQuickWork +0x498, BestTime): 00112620 keeps
   the smaller of it and the race's best lap; 0x157529FF none */
void menu_set_quick_best(uint32_t best);
int menu_course_record_insert(menu *m,const char *course,const char *car,uint32_t lap);
/* Power & Speed's record (unit 0 400 m, 1 1000 m, 2 top speed in m/s,
   3 Las Vegas' 400 m): a time or a speed into its ten */
int menu_machine_record_insert(menu *m,int index,const char *car,uint32_t time,float speed);
/* quick-mt.ad: the session's best top speed (MQuickWork +0x49C, km/h x 100) */
void menu_set_quick_best_speed(int32_t speed);
/* the Time Trial's session has a best lap to save (MQuickWork.canSaveGhost) */
void menu_set_quick_ghost(int on);
/* quick-arcade.ad L159: pending PCM group and raw volume (0..128).
   NULL means no stream request; an empty string means stopStream. */
const char *menu_pcm_request(menu *m,int *raw_volume);
/* The quick menu's command at its finish() (MQuickWork.selectedCommand:
   "COMMAND_START", "COMMAND_REPLAY"...): copied to out and cleared; 0 when
   there is none. */
int menu_take_quick_command(menu *m, char *out, size_t size);
/* tests: a quick menu command as if its icon had been chosen */
void menu_test_quick_command(menu *m,const char *command);
/* tests: the active page's widget of that name focused and chosen */
int menu_test_activate(menu *m,const char *name);
/* a quick menu command waiting to be taken */
int menu_pending_command(const menu *m);
/* A number of the scripts' main.game (e.g. tire_idx), fallback without */
double menu_game_number(menu *m, const char *name, double fallback);
int menu_race_is_split(menu *m);
const char *menu_split_car(menu *m,int port);
/* SplitRoot uses either a catalog label or the selected MCarGarage.
   -1 invalid owned body, 0 catalog, 1 owned; body excludes class header. */
int menu_split_owned(menu *m,int port,uint8_t body[0x4b8],int *color);

/* Virtual loading interval in seconds, from 0.5 to 1.0 per transition. */
float menu_loading_duration(menu *m);

const char *menu_music_name(menu *m); /* NULL keeps prior context music; empty stops */
int menu_take_new_career(menu *m);
int menu_take_quit(menu *m);
/* a GENERIC_CAR label's display name (CAR_NAME), NULL when unknown */
const char *menu_car_name(menu *m,const char *label);
/* the memory card's screen (the memcard project) for replays, the PC's
   files behind it (replay_file.h): the mode a project asked for (the
   Replay Theater's page), opening it on the labomode menu,
   where its finish goes back to, the race a save writes, and a replay
   loaded there (its file name) */
/* the pad buttons held this frame, in the bits the scripts test
   (MENU_PAD_L1, MENU_PAD_R1) */
#define MENU_PAD_L1 0x100u
#define MENU_PAD_R1 0x800u
void menu_set_pad_held(menu *m,unsigned buttons);
int menu_take_labo_arg(menu *m,char *arg,size_t size);
int menu_open_labo(menu *m,const char *arg);
void menu_memcard_return(const char *project);
/* labomode is at its Replay Theater (a replay loaded there plays over it) */
int menu_in_replay_theater(const menu *m);
int menu_take_loaded_replay(char *name,size_t size);
/* 00434770 / 0010BE20: disc attract tapes, separate from user playlists. */
int menu_take_attract_replay(char *name,size_t size);
void menu_attract_finish(menu *m,int cancelled);
int menu_attract_active(void);
/* a best lap the memory card screen loaded (MODE_LOAD_BESTLAP_REPLAY): the
   Time Trial's ghost to take, not a replay to play */
int menu_take_loaded_ghost(char *name,size_t size);
/* the playlist being played stops (its replay left) */
void menu_stop_replay_queue(void);
void menu_set_replay_to_save(const gt4_replay_file *r);

void menu_set_music_fader(menu *m,void (*fn)(float));
/* Start asynchronous preparation at the authored request, before panIn/Sync. */
void menu_set_audio_preparer(menu *m,void (*fn)(menu *,void *),void *user);

int menu_championship_context_get(menu *m,uint8_t live[0x90]);
int menu_championship_context_set(menu *m,const uint8_t live[0x90]);
void menu_pcm_state(menu *m,int playing);
void menu_pcm_parameters(menu *m,int *sequence,int *repeat,unsigned *serial);

#endif /* OPENGT4_MENU_H */
