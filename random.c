#include <stdint.h>
#include <stdio.h>
#include <assert.h>

static uint64_t s_state;
static uint32_t s_sample;
static unsigned s_bits_left;

/* xorshift64s, variant A_1(12,25,27) with multiplier M_32 from line 3 of table 5 */
static uint32_t xorshift64star(uint64_t* state) {
    /* initial seed must be nonzero, don't use a static variable for the state if multithreaded */
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return (x * 0x2545F4914F6CDD1DULL) >> 32;
}

static void refresh()
{
    s_sample = xorshift64star(&s_state);
    s_bits_left = 32;
}

void random_init()
{
    FILE* fp = fopen("/dev/urandom", "r");
    assert(fp != NULL);

    size_t count = fread(&s_state, sizeof(s_state), 1, fp);
    assert(count == 1);
}

void random_uniform(uint8_t end_value, uint8_t* begin, size_t count)
{
    for (size_t idx = 0; idx < count; ++idx) {
        if (s_bits_left < 16) refresh();

        uint32_t v = s_sample & 0xFFFF;
        s_sample >>= 16;
        s_bits_left -= 16;

        *begin++ = (v * end_value) >> 16;
    }
}