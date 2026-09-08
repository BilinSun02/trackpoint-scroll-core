CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS ?= -Iinclude
LDLIBS ?= -lm

BUILD := build
LIBOBJ := $(BUILD)/engine.o $(BUILD)/profiles.o $(BUILD)/rebound.o

.PHONY: all test clean

all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/engine.o: src/engine.c include/trackpoint_scroll/engine.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/profiles.o: src/profiles.c include/trackpoint_scroll/profiles.h include/trackpoint_scroll/engine.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/rebound.o: src/rebound.c include/trackpoint_scroll/rebound.h include/trackpoint_scroll/engine.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/test_engine: tests/test_engine.c $(LIBOBJ) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/test_profiles: tests/test_profiles.c $(LIBOBJ) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/test_rebound: tests/test_rebound.c $(LIBOBJ) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@

test: $(BUILD)/test_engine $(BUILD)/test_profiles $(BUILD)/test_rebound
	./$(BUILD)/test_engine
	./$(BUILD)/test_profiles
	./$(BUILD)/test_rebound

clean:
	rm -rf $(BUILD)
