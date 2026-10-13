/*
 * The Arcade race's results (race_result.h). The board's rows as the
 * ResultArcade object keeps them (rows of 0xC0 from +0xC: the car +0x68,
 * the time text +0x6C, the name +0x9C, the changed flag +0x11C; the
 * player's row +0x4F0, the rows +0x4F4, exit allowed +0x4FC) and its
 * callbacks (func_003DF3B8), the texts from rtext/raceUS.rt2.
 */
#include "race_result.h"
#include "rtext.h"
#include "strobe_player.h"
#include "license_presentation.h"
#include "license_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_TIME 0x157529FFu

typedef struct {
    int car;               /* +0x68, -1 none */
    char time[24];         /* +0x6C */
    char name[96];         /* +0x9C */
    int changed;           /* +0x11C, cb_stable's */
} row;

struct race_result {
    strb_player *movie;
    uint8_t *rt_data;
    gt4_rtext rt;
    int rt_ok;
    row rows[6];
    int nrows;             /* +0x4F4 */
    int player_row;        /* +0x4F0 */
    int done_car[6];       /* +0x14: the car's finish taken */
    int timed[6];          /* +0x2C: its time set */
    uint32_t lead_time;    /* +0x4E8 */
    int lead_laps;         /* +0x4EC */
    int exit_ok;           /* +0x4FC */
    int all_done;          /* +0x48 */
    int over;              /* cb_raceexit */
    race_result_icon icon;
    race_result_sound sound;
    race_result_screen screen;
    void *user;
    int license,license_state,retry,license_first,mission;
    char license_title[96],license_difference[32];
    float trophy_springs[10][4],trophy_dt;
    race_result_trophy trophy;
};

/* func_003E0198: M'SS.mmm, or H:MM'SS.mmm from an hour; none as dashes */
static void format_time(uint32_t ms, char *out, size_t len)
{
    unsigned s, m;
    if (ms == NO_TIME) { snprintf(out, len, "\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f"); return; }
    s = ms / 1000;
    m = s / 60;
    if (m / 60) snprintf(out, len, "%u:%02u'%02u.%03u", m / 60, m % 60, s % 60, ms % 1000);
    else snprintf(out, len, "%u'%02u.%03u", m % 60, s % 60, ms % 1000);
}

/* func_003E0030 (the Arcade's mode 0): the leader's race time; the others'
   gap to it ("+" and the time, func_003E0278), or the laps behind ("%c%d"
   and LAP or LAPS, func_003E0348) */
static void set_time(race_result *r, int at, uint32_t time, int laps)
{
    row *w = &r->rows[at];
    char t[20];
    if (at == 0) { r->lead_time = time; r->lead_laps = laps; }
    if (r->lead_laps == laps) {
        format_time(at == 0 ? time : time - r->lead_time, t, sizeof t);
        snprintf(w->time, sizeof w->time, "%c%s", at == 0 ? ' ' : '+', t);
    } else {
        int behind = r->lead_laps - laps;
        snprintf(w->time, sizeof w->time, "%c%d%s", '+', behind, behind < 2 ? "LAP" : "LAPS");
    }
    w->changed = 1;
}

static int call(void *user, const char *name, const strb_value *args, int nargs, strb_value *ret)
{
    race_result *r = user;
    int at = nargs > 0 ? (int)(args[0].t == STRB_STR ? atof(args[0].s) : args[0].n) : 0;
    const row *w = at >= 0 && at < 6 ? &r->rows[at] : NULL;
    memset(ret, 0, sizeof *ret);
    if(r->license){
        /* 003E0A50 accepts case-insensitive command names (005FBDD0).
           The disc movie calls PrizeLevel, GetTime and EXIT_MENU. */
        char upper[64];size_t i;
        for(i=0;i<sizeof upper-1 && name[i];i++)upper[i]=name[i]>='a' && name[i]<='z'?name[i]-32:name[i];upper[i]=0;
        if(!strcmp(upper,"PRIZELEVEL")){ret->t=STRB_NUM;ret->n=r->license_state==1 || r->license_state==2?0:r->license_state==3?1:r->license_state==4 || r->license_state==5?2:3;return 1;}
        if(!strcmp(upper,"GETTIME")){ret->t=STRB_STR;snprintf(ret->s,sizeof ret->s,"%s",r->rows[0].time);return 1;}
        /* 003E0C18/+0x544, 003E0D10/+0x500, 003E0D68/+0x540. */
        if(!strcmp(upper,"GETTIMEDIF")){ret->t=STRB_STR;snprintf(ret->s,sizeof ret->s,"%s",r->license_difference);return 1;}
        if(!strcmp(upper,"GETTITLE")){ret->t=STRB_STR;snprintf(ret->s,sizeof ret->s,"%s",r->license_title);return 1;}
        if(!strcmp(upper,"GETLECTURE")){ret->t=STRB_STR;return 1;}
        if(!strcmp(upper,"EXIT_MENU")){r->retry=nargs && args[0].t==STRB_STR && !strcmp(args[0].s,"retry");r->over=1;return 1;}
        if(!strcmp(upper,"PAUSEOFF"))return 1;
        if(!strcmp(upper,"PLAY_SOUND")){if(r->sound && nargs && args[0].t==STRB_STR)r->sound(r->user,args[0].s);return 1;}
        /* 003E0DE0..003E0DEC: unregistered commands return undefined. */
        return 1;
    }
    if (!strcmp(name, "cb_stable")) {            /* CB_STABLE: the row changed (then not) */
        ret->t = STRB_BOOL;
        ret->n = w && w->changed;
        if (w) r->rows[at].changed = 0;
    } else if (!strcmp(name, "cb_playerrank")) {  /* CB_PLAYERRANK */
        ret->t = STRB_NUM;
        ret->n = r->player_row;
    } else if (!strcmp(name, "cb_gettime")) {     /* CB_GETTIME */
        ret->t = STRB_STR;
        snprintf(ret->s, sizeof ret->s, "%s", w ? w->time : "");
    } else if (!strcmp(name, "cb_getname")) {     /* CB_GETNAME */
        ret->t = STRB_STR;
        snprintf(ret->s, sizeof ret->s, "%s", w ? w->name : "");
    } else if (!strcmp(name, "cb_isvisible")) {   /* CB_ISVISIBLE */
        ret->t = STRB_BOOL;
        ret->n = at < r->nrows;
    } else if (!strcmp(name, "cb_EnableExit")) {  /* CB_ENABLEEXIT */
        ret->t = STRB_BOOL;
        ret->n = r->exit_ok;
    } else if (!strcmp(name, "cb_raceexit")) {    /* CB_RACEEXIT */
        r->over = 1;
    } else if (!strcmp(name, "isChampionship")) { /* ISCHAMPIONSHIP: the Arcade's single race */
        ret->t = STRB_BOOL;
        ret->n = 0;
    } else if (!strcmp(name, "play_sound")) {
        ret->t = STRB_UNDEF;
        if (r->sound && nargs > 0 && args[0].t == STRB_STR) r->sound(r->user, args[0].s);
    } else return 0;
    return 1;
}

/* the extrender in "_EXT_CARICON<n>": row n's car (func_003DF820) */
static void ext(void *user, const char *name, float x, float y, float w, float h, const uint8_t color[4])
{
    race_result *r = user;
    if(r->license){
        /* 003E0E38 +0x10: screen names the backing race; other authored
           extrenders invoke the trophy. 003E0FF8 draws three samples,
           advancing the springs by half dt between them. */
        if(!strcmp(name,"screen")){
            /* 003E0E38 +0x38..0xa0 transforms both corners, then
               0033DD88 receives x/y and their difference. The movie,
               not the medal or a guessed rectangle, supplies the motion. */
            if(r->screen)r->screen(r->user,x,y,w,h);
            return;
        }
        if(r->license_state<=0 || !r->trophy)return;
        race_result_trophy_pose pose={0};
        static const uint8_t models[]={0,0,0,1,2,2}; /* 006A38F0 */
        pose.model=r->license_state<6?models[r->license_state]:0;
        for(int i=0;i<3;i++){pose.position[i]=r->trophy_springs[i][0];pose.angles[i]=r->trophy_springs[i+3][0];}
        pose.alpha=r->trophy_springs[9][0];
        r->trophy(r->user,&pose);
        gt4_license_trophy_step(r->trophy_springs,r->trophy_dt*.5f);
        gt4_license_trophy_step(r->trophy_springs,r->trophy_dt*.5f);
        return;
    }

    size_t n = strlen(name);
    int at = n ? name[n - 1] - '1' : -1;
    if (!r->icon || at < 0 || at >= r->nrows || r->rows[at].car < 0) return;
    r->icon(r->user, r->rows[at].car, x, y, w, h, color);
}

void race_result_set_sound(race_result *r, race_result_sound sound) { if (r) r->sound = sound; }
void race_result_set_screen(race_result *r,race_result_screen draw){if(r)r->screen=draw;}
void race_result_set_trophy(race_result *r,race_result_trophy draw){if(r)r->trophy=draw;}

static race_result *board_open(gt4_vol *vol, race_result_icon icon, void *user,int mission)
{
    race_result *r = calloc(1, sizeof *r);
    size_t n = 0;
    strb_host host;
    if (!r) return NULL;
    memset(&host, 0, sizeof host);
    r->icon = icon;
    r->mission=mission;
    /* 003DFE20 +28: exit permission is enabled when the board starts,
       independently of unfinished rivals. RaceMission constructs this same
       ResultArcade at world+22778 (003BF6E8 +80..CC). */
    r->exit_ok=mission;
    r->user = user;
    r->rt_data = gt4_vol_read(vol, "/rtext/raceUS.rt2", &n);
    r->rt_ok = r->rt_data && gt4_rtext_open(&r->rt, r->rt_data, n);
    /* func_003E0530: each row empty, its time the race's "Running..."
       (the game's text table D_00621870 +0x134; +0x138/+0x13C the laps') */
    for (int i = 0; i < 6; i++) {
        r->rows[i].car = -1;
        snprintf(r->rows[i].time, sizeof r->rows[i].time, "Running...");
    }
    host.call = call;
    host.ext = ext;
    host.user = r;
    /* 003BF6E8 -> 003DF898 -> 003EC2B8: mission uses the localized movie,
       not a separate invented ResultMission class. */
    r->movie = strb_open(vol, mission?"/menu/US/result.strb":"/menu/common/result.strb", r->rt_ok ? &r->rt : NULL, "RaceDisplay", &host);
    if (!r->movie) {
        race_result_close(r);
        return NULL;
    }
    return r;
}
race_result *race_result_open(gt4_vol *vol,race_result_icon icon,void *user){return board_open(vol,icon,user,0);}
race_result *race_result_mission_open(gt4_vol *vol,race_result_icon icon,void *user){return board_open(vol,icon,user,1);}
race_result *race_result_license_open(gt4_vol *vol,int32_t state,uint32_t time,const char *code,uint32_t bronze,void *user)
{
    race_result *r=calloc(1,sizeof *r);if(!r)return NULL;
    r->license=1;r->license_state=state;r->user=user;r->license_first=1;
    gt4_license_trophy_begin(r->trophy_springs,state);
    format_time(time,r->rows[0].time,sizeof r->rows[0].time);
    size_t n;r->rt_data=gt4_vol_read(vol,"/rtext/raceUS.rt2",&n);
    r->rt_ok=r->rt_data && gt4_rtext_open(&r->rt,r->rt_data,n);
    /* 003BCB38 -> 0038A3D0/0038A3B8 -> 003E0940: original table
       006A0020 (00844340) supplies these rtext keys, format 006A3830.
       Standard examinations have a NULL lecture (003BC320). */
    const char *groups[]={"l0b","l0a","lib","lia","l0s"};
    const char *keys[]={"GTMODE_LICENSE_B","GTMODE_LICENSE_A","GTMODE_LICENSE_IB","GTMODE_LICENSE_IA","GTMODE_LICENSE_S"};
    const char *label=code?code:"";if(strlen(label)>=8)label+=2;
    for(unsigned i=0;i<5;i++)if(!strncmp(label,groups[i],3)){
        const char *title=r->rt_ok?gt4_rtext_get(&r->rt,"RaceDisplay",keys[i]):NULL;
        int number=atoi(label+3);snprintf(r->license_title,sizeof r->license_title,"%s%d\n",title?title:keys[i],number);break;
    }
    /* 003E1EA0 +0x80..0xa8: difference only for timed failures. */
    if(state<=0 && time!=NO_TIME){r->license_difference[0]='+';format_time(time-bronze,r->license_difference+1,sizeof r->license_difference-1);}
    strb_host host={0};host.call=call;host.ext=ext;host.user=r;
    /* 003E1BF8 -> 003DF898 -> 003EC2B8: localized menu/US movie.
       The unlocalized root asset has a different callback contract. */
    r->movie=strb_open(vol,"/menu/US/result_license.strb",r->rt_ok?&r->rt:NULL,"RaceDisplay",&host);
    if(!r->movie){race_result_close(r);return NULL;}
    /* 003DF978 +0xd4 -> 003EBF40 dispatches result input to the movie.
       Reuse the PC STRB vertical focus adapter; its nearest-button rule
       remains A (strobe_player.c), rather than activating both buttons. */
    strb_navigate(r->movie,0);return r;
}

void race_result_close(race_result *r)
{
    if (!r) return;
    strb_close(r->movie);
    free(r->rt_data);
    free(r);
}

void race_result_update(race_result *r, const race_result_state *s, double dt)
{
    int order[6], n;
    if (!r || !s) return;
    if(r->license){
        /* 003E1C28 -> 003DF978 +0xb0: movie clock 3C888888.
           003E1C80/003E1EA0 reset at the outcome AFTER that advance;
           the first outcome draw therefore sees frame zero. */
        r->trophy_dt=gt4_license_result_tick(NULL);
        float movie_dt=gt4_license_result_tick(&r->license_first);
        if(movie_dt>0)strb_tick(r->movie,movie_dt);
        return;
    }
    n = s->cars < 6 ? s->cars : 6;
    r->nrows = n;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && s->rank[order[j]] < s->rank[order[j - 1]]; j--) {
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
        }
    /* func_003DFCC8 at each finish, func_003DFAE0 for the cars running:
       the rows in the race's order, a finished car's row kept */
    for (int at = 0; at < n && !r->all_done; at++) {
        int c = order[at];
        row *w = &r->rows[at];
        if (r->done_car[c]) continue;
        if (s->finished[c]) {
            r->done_car[c] = 1;
            snprintf(w->name, sizeof w->name, "%s", s->name[c] ? s->name[c] : "");
            w->car = c;
            w->changed = 1;
            if (!r->timed[c]) {
                r->timed[c] = 1;
                if (s->mode == 4) {   /* 003E0490: 00472940, +0.005, "%.2f%s" */
                    float v = s->top_speed[c] * 3.6f;
                    if (s->mph) v = v * 0.6213712f;
                    snprintf(w->time, sizeof w->time, "%.2f%s", (double)(v + 0.005f), s->mph ? "mph" : "km/h");
                    w->changed = 1;
                } else set_time(r, at, s->time[c], s->laps[c]);
            }
            r->all_done = 1;
            for (int k = 0; k < n; k++) if (!r->done_car[k]) r->all_done = 0;
            if (r->all_done) r->exit_ok = 1;
        } else if (w->car != c) {
            w->car = c;
            snprintf(w->name, sizeof w->name, "%s", s->name[c] ? s->name[c] : "");
            w->changed = 1;
        }
        if (c == s->player) r->player_row = at;
    }
    /* 003DF978 +B0: original movie clock, independent of host render dt. */
    strb_tick(r->movie, r->mission?gt4_license_result_tick(NULL):dt);
}

void race_result_press(race_result *r) { if (r) { if(r->license)strb_key(r->movie,13);else strb_press(r->movie); } }
void race_result_draw(race_result *r) { if (r) strb_draw(r->movie); }
int race_result_done(const race_result *r) { return r && r->over; }
void race_result_key(race_result *r,int key){if(r)strb_key(r->movie,key);}
int race_result_retry(const race_result *r){return r && r->over && r->retry;}
