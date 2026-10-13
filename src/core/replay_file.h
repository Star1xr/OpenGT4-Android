/* A saved replay (the PC's place of the memory card's FileReplayFull and
   best-lap files, docs/EDITIONS.md: the PC layer). The game replays a race
   by running it again from its start with the player's 0x20-byte command of
   each step (func_003F59A0 -> func_00426C50); the rivals follow from the
   same start (their seeds are their grid slots, func_004639E0). So a file
   holds the race's setup and the commands, not pictures or places. It is
   kept in the save's container (gt4_save_write: CRC, backup, atomic). */
#ifndef OPENGT4_REPLAY_FILE_H
#define OPENGT4_REPLAY_FILE_H
#include <stddef.h>
#include <stdint.h>

#define GT4_REPLAY_OWNED 0x4b8   /* an owned car's body (gt4_race_desc.owned_car) */
#define GT4_REPLAY_ROW 151       /* a decoded RACE row (gt4_race_desc.event_row) */

enum { GT4_REPLAY_FULL = 0, GT4_REPLAY_BEST_LAP = 1 };

typedef struct {
    int kind;                    /* GT4_REPLAY_FULL or _BEST_LAP (the ghost) */
    /* the lineup as the Arcade starts it (race_game_start) */
    char course[32], car[96], others[5][96];
    int color, colors[5], nothers, has_colors;
    int laps, tire, manual, assist_asm, assist_tcs, time_trial;
    /* a career event's prepared grid (race_game_start_prepared) */
    int prepared, championship, cars;
    uint32_t car_id[6], rng;
    uint64_t tuned_car[6], event_paint[6];
    uint8_t event_row[GT4_REPLAY_ROW];
    int owned_on[6];
    uint8_t owned[6][GT4_REPLAY_OWNED];
    /* what a list shows: the player, the race's time and best lap (ms,
       0x157529FF none), when (seconds since 1970, the container's) */
    char player[97], course_name[96], car_name[96];
    uint32_t race_time, best_lap;
    uint64_t when;
    /* the commands, 0x20 bytes a step */
    uint8_t *tape;
    size_t steps;
    /* P: native-file extension for Battle2P. RecorderCommand 00426C50
       stores each human entry separately; the vehicle solver is unchanged. */
    int split_battle, tire_2p, manual_2p, assist_asm_2p, assist_tcs_2p;
    int split_boost, split_handicap, split_tire_damage;
    uint8_t *tape_2p;
    size_t steps_2p;
    /* P: the world's mode when not the path's own (GT Mode's Free Run 11,
       Practice 12: gt4_race_desc.world_mode); 0 in older files */
    int world_mode;
    /* the list's picture (labomode's Icon): RGB, GT4_REPLAY_THUMB_W x _H,
       NULL when none; after the commands, so older files read as before */
    uint8_t *thumb;
    /* the name the player gave it (labomode's Enter Filename, up to 16
       characters); empty for the mode's name */
    char title[64];
    /* a best lap's (the Time Trial's ghost, GT4_REPLAY_BEST_LAP): the
       player's car as it was when the lap began (func_003F8540, the
       ghost's recording header +0x54), the start event's remaining
       thirds and step fraction; the commands are the lap's; best_lap its
       time. NULL in a full replay */
    uint8_t *ghost_car;
    uint32_t ghost_rest;
    float ghost_frac;
    /* Read-only demos from the user's disc. Owned plaintext GT4M header;
       absent in native saved files, never serialized by gt4_replay_pack. */
    uint8_t *disc_info;
    uint8_t *disc_icon; /* owned RGBA 64x64 from icon0.ico; never serialized */
    char license_code[32];
    /* RecorderCommand checkpoints (00346CA0), offsets into disc_info.
       Each segment's own command decoder resets at its declared boundary. */
    int disc_license,disc_event;
    unsigned disc_segments,disc_segments_2p;
    size_t disc_segment_at_2p[3],disc_segment_steps_2p[3];
    uint32_t disc_segment_offset_2p[3];
    size_t disc_segment_at[3],disc_segment_steps[3];
    uint32_t disc_segment_offset[3];
} gt4_replay_file;
#define GT4_REPLAY_GHOST_CAR 0x10a0
#define GT4_REPLAY_THUMB_W 128
#define GT4_REPLAY_THUMB_H 96

/* the record, and back (0 for a damaged or newer one) */
int gt4_replay_pack(const gt4_replay_file *r, uint8_t **out, size_t *size);
int gt4_replay_unpack(const uint8_t *data, size_t size, gt4_replay_file *r);
void gt4_replay_free(gt4_replay_file *r);

/* the files, in the save folder's replays/: a new one (its name out),
   the names newest first, one read, one removed (kept as .deleted) */
int gt4_replay_store(const gt4_replay_file *r, char *name, size_t name_size);
/* an existing file written over (labomode's Overwrite) */
int gt4_replay_store_over(const gt4_replay_file *r, const char *name);
int gt4_replay_list(char (*names)[64], int max);
int gt4_replay_load(const char *name, gt4_replay_file *r);
int gt4_replay_remove(const char *name);
/* the Replay Theater's playlist (the card's "replaylist" file): its
   replays' names in order, in the same folder's playlist file */
int gt4_replay_playlist_read(char (*names)[64], int max);
int gt4_replay_playlist_write(const char (*names)[64], int count);
#endif
