#ifndef INCLUDED_PICKOMINO_H_
#define INCLUDED_PICKOMINO_H_

#include "constants.h"
#include "dice_combinations.h"

#define PICKOMINO_ROLL_REWARD_SCORE_BEGIN 21
#define PICKOMINO_ROLL_REWARD_SCORE_END   37
#define PICKOMINO_MAX_SCORE 40
#define PICKOMINO_ROLL_REWARD_DIM (PICKOMINO_ROLL_REWARD_SCORE_END - PICKOMINO_ROLL_REWARD_SCORE_BEGIN)
#define PICKOMINO_MAX_PLAYERS 4
#define PICKOMINO_TOTAL_DICES 8
#define PICKOMINO_TOTAL_ACTIONS TOTAL_DICE_FACES
#define PICKOMINO_REQUIRED_ACTION (TOTAL_DICE_FACES - 1)
#define PICKOMINO_ROLL_BUSTED 0

extern const uint8_t g_pickomino_face_scores[TOTAL_DICE_FACES];
extern const char g_pickomino_face_symbols[TOTAL_DICE_FACES];
extern const uint8_t g_pickomino_roll_rewards[PICKOMINO_ROLL_REWARD_DIM];

typedef enum pickomino_tile_state_ {
    PICKOMINO_TILE_AVAILABLE,
    PICKOMINO_TILE_OWNED,
    PICKOMINO_TILE_OWNED_HIDDEN,
    PICKOMINO_TILE_REMOVED
} pickomino_tile_state_e;

typedef struct {
    uint8_t player_stacks[PICKOMINO_MAX_PLAYERS][PICKOMINO_ROLL_REWARD_DIM];
    unsigned player_scores[PICKOMINO_MAX_PLAYERS];
    unsigned player_stack_size[PICKOMINO_MAX_PLAYERS];
    pickomino_tile_state_e tile_states[PICKOMINO_ROLL_REWARD_DIM];
    unsigned player_count;
    unsigned cur_player_id;
} pickomino_game_state_s;

typedef struct pickomino_roll_hist_ {
    uint8_t face_idx[TOTAL_DICE_FACES];
    uint8_t face_count[TOTAL_DICE_FACES];
    uint8_t roll_count;
} pickomino_roll_hist_s;

typedef struct pickomino_roll_state_ {
    unsigned score;
    unsigned dices_remaining;
    unsigned used_flags;
    dice_state_s roll_hist;
} pickomino_roll_state_s;

void pickomino_roll_init(pickomino_roll_state_s* r);
unsigned pickomino_roll_available_actions(const pickomino_roll_state_s* r, const dice_state_s* dice);
void pickomino_roll_action(pickomino_roll_state_s* r, const dice_state_s* dice, unsigned action);
void pickomino_roll_format(const dice_state_s* d, char* buf, size_t max_len);
bool pickomino_is_finalizeable(const pickomino_roll_state_s* r);
unsigned pickomino_roll_finalize(const pickomino_roll_state_s* r);

void pickomino_game_init(pickomino_game_state_s* g, int players);
void pickomino_game_process_roll(pickomino_game_state_s* g, unsigned roll_score);
bool pickomino_game_is_done(const pickomino_game_state_s* g);
#endif