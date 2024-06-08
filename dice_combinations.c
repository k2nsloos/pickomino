#include "dice_combinations.h"
#include <stdlib.h>
#include <assert.h>
#include <math.h>


static size_t generate_dice_states(size_t dice_count, dice_state_s* out)
{
    size_t count = 0;
    dice_state_iterator_s it;
    dice_state_iterator_init(&it, dice_count);

    while (!dice_state_iterator_is_end(&it))
    {
        *out++ = *dice_state_iterator_get(&it);
        ++count;
        dice_state_iterator_incr(&it);
    }

    return count;
}

static size_t total_state_count(size_t dice_count, size_t face_count)
{
    size_t numerator = 1;
    size_t denominator = 1;

    while (--face_count) {
        numerator *= dice_count + face_count;
        denominator *= face_count;
    }

    return numerator / denominator;
}

static double calc_state_probability(const dice_state_s* d)
{
    unsigned dice_count = 0;
    double denominator = 1;
    for (size_t idx = 0; idx < TOTAL_DICE_FACES; ++idx) {
        dice_count += d->face_counts[idx];
        denominator *= tgamma(d->face_counts[idx] + 1);
    }

    const double p = 1.0 / TOTAL_DICE_FACES;
    double numerator = tgamma(dice_count + 1) * pow(p, dice_count);
    return numerator / denominator;
}


void dice_state_iterator_init(dice_state_iterator_s* it, size_t dice_count)
{
    *it = (dice_state_iterator_s){.dice_count = dice_count, .elem = {}, .sum = 0};
    it->elem.face_counts[TOTAL_DICE_FACES - 1] = dice_count;
    it->elem.prob = calc_state_probability(&it->elem);
}

bool dice_state_iterator_is_end(const dice_state_iterator_s* it)
{
    return it->sum > it->dice_count;
}

void dice_state_iterator_incr(dice_state_iterator_s* it)
{
    size_t level = TOTAL_DICE_FACES - 2;

    if (it->sum == it->dice_count) {
        size_t face_count;
        while ((face_count = it->elem.face_counts[level]) == 0) {
            --level;
        }

        if (level != 0) {
            it->elem.face_counts[level] = 0;
            it->sum -= face_count;
            --level;
        }
    }

    ++it->elem.face_counts[level];
    ++it->sum;
    it->elem.face_counts[TOTAL_DICE_FACES - 1] = it->dice_count - it->sum;
    it->elem.prob = calc_state_probability(&it->elem);
}

const dice_state_s* dice_state_iterator_get(const dice_state_iterator_s* it)
{
    return &it->elem;
}

dice_state_cache_s* dice_state_cache_create(size_t dice_count)
{
    dice_state_cache_s* c = calloc(1, sizeof(dice_state_cache_s));

    c->count = total_state_count(dice_count, TOTAL_DICE_FACES);
    c->states = calloc(c->count, sizeof(dice_state_s));

    size_t act_len = generate_dice_states(dice_count, c->states);
    assert(act_len == c->count);

    return c;
}

void dice_state_cache_destroy(dice_state_cache_s* c)
{
    if (!c) return;

    free(c->states);
    free(c);
}