#ifndef INCLUDED_DICE_COMBINATIONS_H_
#define INCLUDED_DICE_COMBINATIONS_H_

#include "constants.h"


typedef struct {
    uint8_t face_counts[TOTAL_DICE_FACES];
} dice_state_s;

typedef struct {
    dice_state_s elem;
    unsigned sum;
    unsigned dice_count;
} dice_state_iterator_s;


void dice_state_iterator_init(dice_state_iterator_s* it, size_t dice_count);
bool dice_state_iterator_is_end(const dice_state_iterator_s* it);
void dice_state_iterator_incr(dice_state_iterator_s* it);

const dice_state_s* dice_state_iterator_get(const dice_state_iterator_s* it);
double dice_state_probability(const dice_state_s* it);

dice_state_s* create_dice_states(size_t dice_count, size_t* act_len);

#endif