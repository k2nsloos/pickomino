#include "dice_combinations.h"
#include "random.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

typedef struct
{
    double* value;
    double* p_bust;
    unsigned score;
    unsigned dices_remaining;
    unsigned used_flags;
} game_state_s;

typedef struct
{
    dice_state_s *states;
    double *probs;
    size_t count;
} dice_state_cache_s;

typedef struct
{
    double* values;
    double* p_bust;
    size_t min_score;
    size_t min_dices_remaining;
    size_t dice_dim;
    size_t score_dim;
} game_values_s;

dice_state_cache_s s_state_caches[TOTAL_DICES];
const uint8_t s_face_scores[TOTAL_DICE_FACES] = { 1, 2, 3, 4, 5, 5 };
const char s_face_symbols[TOTAL_DICE_FACES] = { '1', '2', '3', '4', '5', 'W' };
game_values_s *g_state;
size_t g_state_size;

static unsigned pop_count(unsigned v)
{
    unsigned c = 0;
    do { c += (v & 1); } while (v >>= 1);
    return c;
}

static void find_game_state(game_state_s* s, unsigned score, unsigned dices_remaining, unsigned used_flags)
{
    game_values_s* values = &g_state[used_flags];
    size_t score_idx = score - values->min_score;
    size_t dices_idx = dices_remaining - values->min_dices_remaining;

    size_t offset = dices_idx * values->score_dim + score_idx;
    assert(score_idx < values->score_dim);
    assert(dices_idx < values->dice_dim);

    *s = (game_state_s) {.value = &values->values[offset],
                         .p_bust = &values->p_bust[offset],
                         .score = score,
                         .dices_remaining = dices_remaining,
                         .used_flags = used_flags
    };
}

static void get_game_state_by_idx(game_state_s* s, size_t game_idx)
{
    uint16_t offset = game_idx;
    uint8_t used_flags = game_idx >> 16;
    assert(used_flags < TOTAL_USED_STATES);

    game_values_s* values = &g_state[used_flags];

    assert(offset < values->score_dim * values->dice_dim);
    unsigned score = offset % values->score_dim + values->min_score;
    unsigned dices_remaining = offset / values->score_dim + values->min_dices_remaining;

    *s = (game_state_s) {
        .value = &values->values[offset],
        .p_bust = &values->p_bust[offset],
        .score = score,
        .dices_remaining = dices_remaining,
        .used_flags = used_flags
    };
}


static void setup_state_cache()
{
    for (size_t dice_idx = 0; dice_idx < TOTAL_DICES; ++dice_idx) {
        dice_state_cache_s* cache = &s_state_caches[dice_idx];
        cache->states = create_dice_states(dice_idx + 1, &cache->count);
        cache->probs = calloc(cache->count, sizeof(double));
        for (size_t state_idx = 0; state_idx < cache->count; ++state_idx) {
            cache->probs[state_idx] = dice_state_probability(&cache->states[state_idx]);
        }
    }
}

static size_t prepare_game_values(game_values_s* v, unsigned flags)
{
    size_t min_score = 0;
    size_t max_dice_idx = 0;
    size_t min_dice_used = 0;
    for (size_t idx = 0; idx < TOTAL_DICE_FACES; ++idx)
    {
        if (((1u << idx) & flags) == 0) continue;

        ++min_dice_used;
        max_dice_idx = idx;
        min_score += s_face_scores[idx];
    }

    size_t max_dice = TOTAL_DICES - min_dice_used;
    v->min_dices_remaining = flags ? 0 : TOTAL_DICES;
    v->dice_dim = max_dice - v->min_dices_remaining + 1;

    v->min_score = min_score;
    //size_t max_score = min_score + max_dice * s_face_scores[max_dice_idx];
    size_t max_score = MAX_SCORE;
    v->score_dim = max_score - v->min_score + 1;

    size_t state_size = v->score_dim * v->dice_dim;
    v->values = calloc(state_size, sizeof(double));
    v->p_bust = calloc(state_size, sizeof(double));

    (void)max_dice_idx;
    return state_size;
}

static size_t setup()
{
    g_state_size = 0;
    g_state = calloc(TOTAL_USED_STATES, sizeof(game_values_s));

    for (size_t used_flag = 0; used_flag < TOTAL_USED_STATES; ++used_flag)
    {
        size_t sub_state_size = prepare_game_values(&g_state[used_flag], used_flag);
        g_state_size += sub_state_size;

        size_t mask = used_flag << 16;
        for (size_t game_idx = 0; game_idx < sub_state_size; ++game_idx) {
            game_state_s s;
            get_game_state_by_idx(&s, mask | game_idx);

            // If required face present we can always stop
            if (s.used_flags & (1u << REQUIRED_FACE)) *s.value = s.score;
        }
    }

    setup_state_cache();
    return g_state_size;
}

static const char* format_state(const game_state_s* g)
{
    static char tpl[32];
    snprintf(tpl, sizeof(tpl),
             "(%d, %d, %.1f, %.2f %d%d%d%d%d%d)",
             g->score,
             g->dices_remaining,
             *g->value,
             *g->p_bust,
             (bool)(g->used_flags & 1),
             (bool)(g->used_flags & 2),
             (bool)(g->used_flags & 4),
             (bool)(g->used_flags & 8),
             (bool)(g->used_flags & 16),
             (bool)(g->used_flags & 32));

    return tpl;
}

static void update(const game_state_s* src_game)
{
    game_state_s dst_game;

    bool has_required_face = src_game->used_flags & (1u << REQUIRED_FACE);
    bool is_allowed_to_stop = has_required_face && src_game->score >= MIN_STOP_SCORE;
    double state_stop_value = is_allowed_to_stop ? src_game->score : 0;
    double state_stop_p_bust = is_allowed_to_stop ? 0 : 1;

    double state_roll_value = 0;
    double state_roll_p_bust = 1.0;
    if (src_game->dices_remaining > 0 && src_game->used_flags != TOTAL_USED_STATES - 1) {
        const dice_state_cache_s* dice_cache = &s_state_caches[src_game->dices_remaining - 1];
        state_roll_p_bust = 0;

        for (size_t dice_idx = 0; dice_idx < dice_cache->count; ++dice_idx) {
            const dice_state_s* dice = &dice_cache->states[dice_idx];
            double p = dice_cache->probs[dice_idx];

            bool have_action = false;
            double max_state_action_value = 0;
            double max_state_action_p_bust = 1.0;
            for (size_t action = 0; action < TOTAL_DICE_FACES; ++action) {
                if (src_game->used_flags & (1u << action)) continue;
                if (dice->face_counts[action] == 0) continue;

                double new_score = src_game->score + s_face_scores[action] * dice->face_counts[action];
                if (new_score > MAX_SCORE) break; // Unreachable state, don't care

                unsigned new_dice_remaining = src_game->dices_remaining - dice->face_counts[action];
                unsigned new_used = src_game->used_flags | (1u << action);
                find_game_state(&dst_game, new_score, new_dice_remaining, new_used);

                if (!have_action || *dst_game.value > max_state_action_value) {
                    max_state_action_value = *dst_game.value;
                    max_state_action_p_bust = *dst_game.p_bust;
                    have_action = true;
                }
            }

            state_roll_value += p * max_state_action_value;
            state_roll_p_bust += p * max_state_action_p_bust;
        }
    }

    double new_value = state_stop_value;
    double new_p_bust = state_stop_p_bust;

    if (state_roll_value > state_stop_value) {
        new_value = state_roll_value;
        new_p_bust = state_roll_p_bust;
    }

    // const char* state_str = format_state(src_game);
    // printf("  %s: %f -> %f\n", state_str, *src_game->value, new_value);
    *src_game->value = new_value;
    *src_game->p_bust = new_p_bust;
}



static const char* format_roll(const dice_state_s* d)
{
    static char tpl[32];
    char* cur = tpl;

    for (size_t face_idx = TOTAL_DICE_FACES; face_idx--; ) {
        for (size_t count = 0; count < d->face_counts[face_idx]; ++count) {
            *cur++ = s_face_symbols[face_idx];
            *cur++ = ' ';
        }
    }

    if (cur != tpl) --cur;
    *cur = 0;
    return tpl;
}

void do_random_roll(dice_state_s* result, size_t dices)
{
    assert(dices <= TOTAL_DICES);

    dice_state_s tmp = {};
    uint8_t rolls[TOTAL_DICES] = {};
    random_uniform(TOTAL_DICE_FACES, rolls, dices);
    for (size_t idx = 0; idx < dices; ++idx) {
        ++tmp.face_counts[rolls[idx]];
    }
    *result = tmp;
}

static void play_game()
{
    game_state_s game;
    game_state_s max_action;
    game_state_s tmp;
    size_t max_action_idx = 0;

    find_game_state(&game, 0, TOTAL_DICES, 0);

    while (true)
    {
        printf("state: %s\n", format_state(&game));

        if (*game.value == game.score) {
            printf("stop\n");
            break;
        }

        dice_state_s dice;
        random_init();
        do_random_roll(&dice, game.dices_remaining);
        printf("roll: %s\n", format_roll(&dice));

        bool have_action = false;
        double max_state_action_value = 0;
        for (size_t action = 0; action < TOTAL_DICE_FACES; ++action) {
            if (game.used_flags & (1u << action)) continue;
            if (dice.face_counts[action] == 0) continue;

            double new_score = game.score + s_face_scores[action] * dice.face_counts[action];
            unsigned new_dice_remaining = game.dices_remaining - dice.face_counts[action];
            unsigned new_used = game.used_flags | (1u << action);
            find_game_state(&tmp, new_score, new_dice_remaining, new_used);

            if (!have_action || *tmp.value > max_state_action_value) {
                max_action = tmp;
                max_state_action_value = *tmp.value;
                max_action_idx = action;
                have_action = true;
            }

            printf("action: %c -> %s\n", s_face_symbols[action], format_state(&tmp));
        }

        if (!have_action) {
            printf("bust!\n");
            break;
        }

        game = max_action;
        printf("do: %c\n\n", s_face_symbols[max_action_idx]);
    }
}

int main(int argc, char **argv)
{
    const size_t state_size = setup();
    printf("State space size: %u\n", (unsigned)state_size);

    size_t round = 1;
    for (size_t flag_count = TOTAL_DICE_FACES + 1; flag_count-- != 0; ++round)
    {
        printf("Round %d\n", (int)round);
        for (size_t used_flags = 0; used_flags < TOTAL_USED_STATES; ++used_flags)
        {
            if (pop_count(used_flags) != flag_count) continue;
            const game_values_s* v = &g_state[used_flags];
            size_t sub_state_size = v->dice_dim * v->score_dim;
            size_t mask = used_flags << 16;

            for (size_t game_idx = 0; game_idx < sub_state_size; ++game_idx) {
                game_state_s game;
                get_game_state_by_idx(&game, game_idx | mask);
                update(&game);
            }
        }
    }

    play_game();
}
