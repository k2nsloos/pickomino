#include "random.h"
#include "dice_combinations.h"

#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

#define SAMPLE_COUNT 1000000

int s_face_counts[TOTAL_DICE_FACES];

static void setup()
{
    uint8_t* samples = calloc(SAMPLE_COUNT, sizeof(uint8_t));

    random_init();
    random_uniform(TOTAL_DICE_FACES, samples, SAMPLE_COUNT);

    for (size_t idx = 0; idx < TOTAL_DICE_FACES; ++idx) {
        s_face_counts[idx] = 0;
    }

    for (size_t idx = 0; idx < SAMPLE_COUNT; ++idx) {
        uint8_t sample = samples[idx];
        assert(sample < TOTAL_DICE_FACES);
        ++s_face_counts[sample];
    }
}

static void test_stddev()
{
    const double exp_avg = SAMPLE_COUNT / 6.0;
    const double exp_stddev = sqrt(5 * SAMPLE_COUNT) / 6;
    const int max_deviation = (int)4 * exp_stddev;

    setup();
    printf("%u +/- %d\n", (unsigned)round(exp_avg), max_deviation);


    for (size_t idx = 0; idx < TOTAL_DICE_FACES; ++idx)
    {
        printf("%u %u %d\n", (unsigned)idx + 1, s_face_counts[idx], (int)round(s_face_counts[idx] - exp_avg));
        assert(fabs(s_face_counts[idx] - exp_avg) <= max_deviation);
    }
}

int main(int argc, char **argv)
{
    test_stddev();
    return 0;
}