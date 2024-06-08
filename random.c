#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#define BUF_SIZE 1024

static uint16_t s_buf[BUF_SIZE];
static uint16_t* s_cur;

static void refresh()
{
    FILE* fp = fopen("/dev/urandom", "r");
    assert(fp != NULL);

    size_t count = fread(&s_buf, sizeof(s_buf), 1, fp);
    assert(count == 1);
}

void random_init()
{
    FILE* fp = fopen("/dev/urandom", "r");
    assert(fp != NULL);

    size_t count = fread(&s_buf, sizeof(s_buf), 1, fp);
    assert(count == 1);

    s_cur = s_buf;
}

void random_uniform(uint8_t end_value, uint8_t* begin, size_t count)
{
    for (size_t idx = 0; idx < count; ++idx) {
        if (s_cur - s_buf >= BUF_SIZE) refresh();
        uint32_t v = *s_cur++;
        *begin++ = (v * end_value) >> 16;
    }
}