
SHELL=/bin/bash
CC=gcc
CFLAGS=-std=c11 -Wall -ggdb -Og -I.

.PHONY: directories all clean %.test

all: directories \
     build/maximize_score

clean:
	@rm -rf build

test: directories test_dice_combo.test

directories:
	@mkdir -p build
	@mkdir -p build/test

build/%.o: %.c
	@echo "[CC]   $<"
	@$(CC) -c $(CFLAGS) -o $@ $<

build/maximize_score: build/maximize_score.o build/dice_combinations.o build/random.o
	@echo "[Link] $@"
	@$(CC) $(CFLAGS) -o $@ $^ -lm

build/test/test_dice_combo: build/test/test_dice_combo.o build/random.o
	@echo "[Link] $@"
	@$(CC) $(CFLAGS) -o $@ $^ -lm

%.test: build/test/%
	@echo "[Run]  $<"
	@$< > /dev/null || (echo FAILED $< && exit 1)