#include "pickomino.h"
#include "dice_combinations.h"
#include "random.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define TOTAL_USED_STATES 64
#define REQUIRED_FACE (TOTAL_DICE_FACES - 1)
#define MIN_STOP_SCORE 21

typedef struct
{
    double value;
    double p_score[PICKOMINO_ROLL_REWARD_DIM];
    double p_bust;
} roll_stats_s;

typedef struct
{
    roll_stats_s *values;
    unsigned min_score;
    unsigned score_dim;
} roll_stats_dice_dim_s;

typedef struct
{
    roll_stats_dice_dim_s* values;
    size_t min_dice_remaining;
    size_t dice_dim;
    size_t total_stats_count;
} roll_stats_flags_dim_s;

static dice_state_cache_s* s_dice_states[PICKOMINO_TOTAL_DICES];
static roll_stats_flags_dim_s s_roll_stats[TOTAL_USED_STATES];
size_t g_total_roll_stats_count;

static unsigned pop_count(unsigned v)
{
    unsigned c = 0;
    do { c += (v & 1); } while (v >>= 1);
    return c;
}

static void roll_stats_dice_dim_init(roll_stats_dice_dim_s* l, size_t min_score, size_t max_score)
{
    l->min_score = min_score;
    l->score_dim = (max_score - min_score) + 1;
    l->values = calloc(l->score_dim, sizeof(roll_stats_s));
}

static void roll_stats_flags_dim_init(roll_stats_flags_dim_s* r, unsigned used_flags)
{
    size_t min_dice_idx = SIZE_MAX;
    size_t max_dice_idx = SIZE_MAX;
    size_t min_score = 0;
    size_t min_dice_used = 0;

    for (size_t idx = 0; idx < TOTAL_DICE_FACES; ++idx) {
        if (((1u << idx) & used_flags) == 0) continue;
        if (min_dice_idx != SIZE_MAX) min_dice_idx = idx;

        ++min_dice_used;
        max_dice_idx = idx;
        min_score += g_pickomino_face_scores[idx];
    }

    size_t max_dice_used = used_flags == 0 ? 0 : PICKOMINO_TOTAL_DICES;
    size_t dice_dim = (max_dice_used - min_dice_used) + 1;

    r->values = calloc(dice_dim, sizeof(roll_stats_dice_dim_s));
    r->dice_dim = dice_dim;
    r->min_dice_remaining = PICKOMINO_TOTAL_DICES - max_dice_used;

    size_t min_score_per_dice = min_dice_idx == SIZE_MAX ? 0 : g_pickomino_face_scores[min_dice_idx];
    size_t max_score_per_dice = max_dice_idx == SIZE_MAX ? 0 : g_pickomino_face_scores[max_dice_idx];
    for (size_t idx = 0; idx < dice_dim; ++idx) {
        size_t list_min_score = min_score + min_score_per_dice * (dice_dim - idx - 1);
        size_t list_max_score = min_score + max_score_per_dice * (dice_dim - idx - 1);
        roll_stats_dice_dim_init(&r->values[idx], list_min_score, list_max_score);
        r->total_stats_count += r->values[idx].score_dim;
    }
}

static void setup()
{
    for (size_t dice_id = 0; dice_id < PICKOMINO_TOTAL_DICES; ++dice_id) {
        s_dice_states[dice_id] = dice_state_cache_create(dice_id + 1);
    }

    g_total_roll_stats_count = 0;
    for (unsigned flags = 0; flags < TOTAL_USED_STATES; ++flags) {
        roll_stats_flags_dim_init(&s_roll_stats[flags], flags);
        g_total_roll_stats_count += s_roll_stats[flags].total_stats_count;
    }
}

static roll_stats_s* find_roll_stats(const pickomino_roll_state_s* s)
{
    assert(s->used_flags < TOTAL_USED_STATES);
    const roll_stats_flags_dim_s* l1 = &s_roll_stats[s->used_flags];

    assert(s->dices_remaining - l1->min_dice_remaining < l1->dice_dim);
    const roll_stats_dice_dim_s* l2 = &l1->values[s->dices_remaining - l1->min_dice_remaining];

    assert(s->score - l2->min_score < l2->score_dim);
    return &l2->values[s->score - l2->min_score];
}

roll_stats_dice_dim_s* find_roll_stats_dice_dim(unsigned used_flags, unsigned dices_remaining)
{
    assert(used_flags < TOTAL_USED_STATES);
    const roll_stats_flags_dim_s* l1 = &s_roll_stats[used_flags];

    assert(dices_remaining - l1->min_dice_remaining < l1->dice_dim);
    return &l1->values[dices_remaining - l1->min_dice_remaining];
}

static const char* format_state(const pickomino_roll_state_s* state)
{
    static char tpl[32];
    roll_stats_s* stats = find_roll_stats(state);
    snprintf(tpl, sizeof(tpl),
             "(%d, %d, %.1f, %.2f %d%d%d%d%d%d)",
             state->score,
             state->dices_remaining,
             stats->value,
             stats->p_bust,
             (bool)(state->used_flags & 1),
             (bool)(state->used_flags & 2),
             (bool)(state->used_flags & 4),
             (bool)(state->used_flags & 8),
             (bool)(state->used_flags & 16),
             (bool)(state->used_flags & 32));
    return tpl;
}

static void update(const pickomino_roll_state_s* src_game)
{
    bool has_required_face = src_game->used_flags & (1u << REQUIRED_FACE);
    bool is_allowed_to_stop = has_required_face && src_game->score >= MIN_STOP_SCORE;
    double state_stop_value = is_allowed_to_stop ? src_game->score : 0;
    double state_stop_p_bust = is_allowed_to_stop ? 0 : 1;

    double state_roll_value = 0;
    double state_roll_p_bust = 1.0;

    if (src_game->dices_remaining > 0 && src_game->used_flags != TOTAL_USED_STATES - 1) {
        const dice_state_cache_s* dice_cache = s_dice_states[src_game->dices_remaining - 1];
        state_roll_p_bust = 0;

        for (size_t dice_idx = 0; dice_idx < dice_cache->count; ++dice_idx) {
            const dice_state_s* dice = &dice_cache->states[dice_idx];

            bool have_action = false;
            double max_state_action_value = 0;
            double max_state_action_p_bust = 1.0;
            for (size_t action = 0; action < TOTAL_DICE_FACES; ++action) {
                if (src_game->used_flags & (1u << action)) continue;
                if (dice->face_counts[action] == 0) continue;

                pickomino_roll_state_s dst_game = *src_game;
                pickomino_roll_action(&dst_game, dice, action);
                roll_stats_s* dst_stats = find_roll_stats(&dst_game);

                if (!have_action || dst_stats->value > max_state_action_value) {
                    max_state_action_value = dst_stats->value;
                    max_state_action_p_bust = dst_stats->p_bust;
                    have_action = true;
                }
            }

            state_roll_value += dice->prob * max_state_action_value;
            state_roll_p_bust += dice->prob * max_state_action_p_bust;
        }
    }

    double new_value, new_p_bust;
    if (state_roll_value > state_stop_value) {
        new_value = state_roll_value;
        new_p_bust = state_roll_p_bust;
    } else {
        new_value = state_stop_value;
        new_p_bust = state_stop_p_bust;
    }

    // const char* state_str = format_state(src_game);
    // printf("  %s: %f -> %f\n", state_str, *src_game->value, new_value);
    roll_stats_s* src_stats = find_roll_stats(src_game);
    src_stats->value = new_value;
    src_stats->p_bust = new_p_bust;
}

static const char* format_roll(const dice_state_s* d)
{
    static char tpl[32];
    pickomino_roll_format(d, tpl, sizeof(tpl));
    return tpl;
}

void do_random_roll(dice_state_s* result, size_t dices)
{
    assert(dices <= PICKOMINO_TOTAL_DICES);

    dice_state_s tmp = {.face_counts = {}};
    uint8_t rolls[PICKOMINO_TOTAL_DICES] = {};
    random_uniform(TOTAL_DICE_FACES, rolls, dices);
    for (size_t idx = 0; idx < dices; ++idx) {
        ++(tmp.face_counts[rolls[idx]]);
    }
    *result = tmp;
}

static void play_game()
{
    pickomino_roll_state_s game = {0, PICKOMINO_TOTAL_DICES, 0, {}};
    pickomino_roll_state_s max_action;
    pickomino_roll_state_s tmp;
    size_t max_action_idx = 0;

    while (true)
    {
        printf("state: %s\n", format_state(&game));


        if (find_roll_stats(&game)->value == game.score) {
            printf("stop\n");
            break;
        }

        dice_state_s dice;
        random_init();
        do_random_roll(&dice, game.dices_remaining);
        printf("roll: %s\n", format_roll(&dice));

        bool have_action = false;
        double max_state_action_value = 0;

        unsigned available_actions = pickomino_roll_available_actions(&game, &dice);
        for (unsigned action = 0; available_actions; available_actions >>= 1, ++action) {
            if ((available_actions & 0x1) == 0) continue;

            tmp = game;
            pickomino_roll_action(&tmp, &dice, action);
            roll_stats_s* stats = find_roll_stats(&tmp);

            if (!have_action || stats->value > max_state_action_value) {
                max_action = tmp;
                max_state_action_value = stats->value;
                max_action_idx = action;
                have_action = true;
            }

            printf("action: %c -> %s\n", g_pickomino_face_symbols[action], format_state(&tmp));
        }

        if (!have_action) {
            printf("bust!\n");
            break;
        }

        game = max_action;
        printf("do: %c\n\n", g_pickomino_face_symbols[max_action_idx]);
    }
}

int main(int argc, char **argv)
{
    setup();
    printf("State space size: %u\n", (unsigned)g_total_roll_stats_count);

    size_t round = 1;
    for (size_t flag_count = TOTAL_DICE_FACES + 1; flag_count-- != 0; ++round)
    {
        printf("Round %d\n", (int)round);
        for (size_t used_flags = 0; used_flags < TOTAL_USED_STATES; ++used_flags)
        {
            if (pop_count(used_flags) != flag_count) continue;
            roll_stats_flags_dim_s* roll_stats_flags_dim = &s_roll_stats[used_flags];
            for (size_t dice_id = 0; dice_id < roll_stats_flags_dim->dice_dim; ++dice_id) {
                roll_stats_dice_dim_s* roll_stats_dice_dim = &roll_stats_flags_dim->values[dice_id];
                size_t dice_remaining = roll_stats_flags_dim->min_dice_remaining + dice_id;
                for (size_t score_id = 0; score_id < roll_stats_dice_dim->score_dim; ++score_id) {
                    pickomino_roll_state_s state = {
                        .dices_remaining = dice_remaining,
                        .score = roll_stats_dice_dim->min_score + score_id,
                        .used_flags = used_flags,
                        .roll_hist = {},
                    };
                    update(&state);
                }
            }
        }
    }

    play_game();
}
