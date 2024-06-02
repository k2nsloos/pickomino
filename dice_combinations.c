#include "dice_combinations.h"
#include <stdlib.h>
#include <assert.h>
#include <math.h>

void dice_state_iterator_init(dice_state_iterator_s* it, size_t dice_count)
{
    *it = (dice_state_iterator_s){.dice_count = dice_count, .elem = {}, .sum = 0};
    it->elem.face_counts[TOTAL_DICE_FACES - 1] = dice_count;
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
}

const dice_state_s* dice_state_iterator_get(const dice_state_iterator_s* it)
{
    return &it->elem;
}

double dice_state_probability(const dice_state_s* d)
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

static size_t generate_dice_states(size_t dice_count, dice_state_s* out)
{
    size_t count = 0;
    dice_state_iterator_s it;
    dice_state_iterator_init(&it, dice_count);

    while (!dice_state_iterator_is_end(&it))
    {
        const dice_state_s* s = dice_state_iterator_get(&it);
        *out++ = *s;
        ++count;
        dice_state_iterator_incr(&it);
    }

    return count;
}


static size_t total_state_count(size_t dice_count, size_t face_count)
{
    size_t numerator = 1;
    size_t denominator = 1;

    while (--face_count)
    {
        numerator *= dice_count + face_count;
        denominator *= face_count;
    }

    return numerator / denominator;
}

dice_state_s* create_dice_states(size_t dice_count, size_t* act_len)
{
    size_t state_count = total_state_count(dice_count, TOTAL_DICE_FACES);
    dice_state_s* result = calloc(state_count, sizeof(dice_state_s));
    *act_len = generate_dice_states(dice_count, result);
    assert(state_count == *act_len);
    return result;
}

/*
#include <stdio.h>
int main(int argc, char** argv)
{
    size_t act_len = 0;

    for (size_t dice_count = 1; dice_count < 9; ++dice_count) {
        dice_state_s* states = create_dice_states(dice_count, &act_len);
        printf("%lu -> %lu\n", dice_count, act_len);
    }
}*/