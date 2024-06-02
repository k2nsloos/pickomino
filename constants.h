#ifndef INCLUDED_CONSTANTS_H_
#define INCLUDED_CONSTANTS_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) < (B) ? (B) : (A))

#define TOTAL_DICES 8
#define TOTAL_DICE_FACES 6

#define TOTAL_USED_STATES 64
#define MIN_STOP_SCORE 21
#define MAX_SCORE 40
#define REQUIRED_FACE 5

#endif