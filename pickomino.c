#include "pickomino.h"
#include <assert.h>

#define PICKOMINO_TILE_NONE 0xFF

const uint8_t g_pickomino_face_scores[TOTAL_DICE_FACES] = { 1, 2, 3, 4, 5, 5 };
const char g_pickomino_face_symbols[TOTAL_DICE_FACES] = {'1', '2', '3', '4', '5', 'W'};

const uint8_t g_pickomino_roll_rewards[PICKOMINO_ROLL_REWARD_DIM] = {
    1, 1, 1, 1,
    2, 2, 2, 2,
    3, 3, 3, 3,
    4, 4, 4, 4,
};


void pickomino_roll_init(pickomino_roll_state_s* r)
{
    *r = (pickomino_roll_state_s){
        .score = 0,
        .dices_remaining = PICKOMINO_TOTAL_DICES,
        .used_flags = 0,
        .roll_hist = {}
    };
}

unsigned pickomino_roll_available_actions(const pickomino_roll_state_s* r, const dice_state_s* dice)
{
    unsigned result = 0;
    for (size_t action = 0; action < PICKOMINO_TOTAL_ACTIONS; ++action) {
        unsigned mask = 1u << action;
        if (r->used_flags & mask) continue;
        if (!dice->face_counts[action]) continue;

        result |= mask;
    }

    return result;
}

void pickomino_roll_action(pickomino_roll_state_s* r, const dice_state_s* dice, unsigned action)
{
    assert((r->used_flags & (1u << action)) == 0);
    assert(r->dices_remaining >= dice->face_counts[action]);

    r->score += dice->face_counts[action] * g_pickomino_face_scores[action];
    r->dices_remaining -= dice->face_counts[action];
    r->used_flags |= 1u << action;
    r->roll_hist.face_counts[action] = dice->face_counts[action];
}

void pickomino_roll_format(const dice_state_s* d, char* buf, size_t max_len)
{
    const char* end = buf + max_len;
    bool trim_final_space = false;

    for (size_t face_id = TOTAL_DICE_FACES; face_id--; ) {
        if (end - buf < 2) break;
        if (!d->face_counts[face_id]) continue;

        for (size_t count = d->face_counts[face_id]; count--; ) {
            if (end - buf < 2) break;
            *buf++ = g_pickomino_face_symbols[face_id];
            *buf++ = ' ';
            trim_final_space = true;
        }
    }

    if (trim_final_space) *--buf = 0;
}

bool pickomino_is_finalizeable(const pickomino_roll_state_s* r)
{
    return r->used_flags & (1u << PICKOMINO_REQUIRED_ACTION);
}

unsigned pickomino_roll_finalize(const pickomino_roll_state_s* r)
{
    return pickomino_is_finalizeable(r) ? r->score : PICKOMINO_ROLL_BUSTED;
}

void pickomino_game_init(pickomino_game_state_s* g, int players)
{
    *g = (pickomino_game_state_s){.player_count = players};
}


static uint8_t peek_tile_stack(const pickomino_game_state_s* g, size_t player_id)
{
    unsigned stack_size = g->player_stack_size[player_id];
    if (!stack_size) return PICKOMINO_TILE_NONE;

    uint8_t top_tile = g->player_stacks[player_id][stack_size - 1];
    return top_tile;
}

static void pop_tile_stack(pickomino_game_state_s* g, size_t player_id)
{
    unsigned stack_size = g->player_stack_size[player_id];
    if (!stack_size) return;

    uint8_t top_tile = g->player_stacks[player_id][stack_size - 1];
    --g->player_stack_size[player_id];

    g->player_scores[player_id] -= g_pickomino_roll_rewards[top_tile];
    g->tile_states[top_tile] = PICKOMINO_TILE_AVAILABLE;
}

static void push_tile_stack(pickomino_game_state_s* g, size_t player_id, uint8_t tile)
{
    unsigned stack_size = g->player_stack_size[player_id]++;
    g->tile_states[tile] = PICKOMINO_TILE_OWNED;
    g->player_stacks[player_id][stack_size] = tile;
    g->player_scores[player_id] += g_pickomino_roll_rewards[tile];
}

static void remove_top_tile(pickomino_game_state_s* g, uint8_t returned_tile)
{
    // Remove top tile (if it was not just removed)
    for (size_t idx = PICKOMINO_ROLL_REWARD_DIM; idx-- != 0; ) {
        if (g->tile_states[idx] == PICKOMINO_TILE_AVAILABLE) {
            if (returned_tile != idx) g->tile_states[idx] = PICKOMINO_TILE_REMOVED;
            break;
        }
    }
}

static bool try_steal_tile(pickomino_game_state_s* g, uint8_t tile)
{
    for (size_t player_id = 0; player_id < g->player_count; ++player_id) {
        if (peek_tile_stack(g, player_id) == tile) {
            pop_tile_stack(g, player_id);
            push_tile_stack(g, g->cur_player_id, tile);
            return true;
        }
    }

    return false;
}

static bool pick_closest_tile(pickomino_game_state_s* g, uint8_t tile)
{
    for (size_t idx = tile; idx-- != 0; ) {
        if (g->tile_states[idx] == PICKOMINO_TILE_AVAILABLE) {
            push_tile_stack(g, g->cur_player_id, idx);
            return true;
        }
    }
    return false;
}

static void process_bust(pickomino_game_state_s* g)
{
    uint8_t returned_tile = peek_tile_stack(g, g->cur_player_id);
    if (returned_tile != PICKOMINO_TILE_NONE) {
        pop_tile_stack(g, g->cur_player_id);
        remove_top_tile(g, returned_tile);
    }
}

void pickomino_game_process_roll(pickomino_game_state_s* g, unsigned roll_score)
{
    if (roll_score >= PICKOMINO_ROLL_REWARD_SCORE_BEGIN) {
        uint8_t tile = roll_score - PICKOMINO_ROLL_REWARD_SCORE_BEGIN;
        if (try_steal_tile(g, tile)) return;
        if (pick_closest_tile(g, tile)) return;
    }

    process_bust(g);
}

bool pickomino_game_is_done(const pickomino_game_state_s* g)
{
    for (size_t idx = PICKOMINO_ROLL_REWARD_DIM; idx-- != 0; ) {
        if (g->tile_states[idx] == PICKOMINO_TILE_AVAILABLE) return false;
    }
    return true;
}