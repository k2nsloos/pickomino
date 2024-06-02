#ifndef INCLUDED_RANDOM_H_
#define INCLUDED_RANDOM_H_

#include <stdint.h>
#include <stddef.h>

void random_init();
void random_uniform(uint8_t end_value, uint8_t* begin, size_t count);

#endif