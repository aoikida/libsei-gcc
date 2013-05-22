# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

CFLAGS  = -g -O0 -DDEBUG=0 -Wall
#CFLAGS  = -O3
CFLAGS += -DASCO_COMPLETE -DTMASCO_ENABLED
BUILD   = build
SRCS    = heap.c cow.c asco.c
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.o))
TARGET  = libasco.a
TARGET2 = libtmasco.a

# $(info $(OBJS))

.PHONY: all clean

all: $(BUILD)/$(TARGET) $(BUILD)/$(TARGET2)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o : src/%.c | $(BUILD)
	gcc $(CFLAGS) -I include -c -o $@ $<

$(BUILD)/$(TARGET): $(OBJS)
	ar rvs $@ $^

$(BUILD)/$(TARGET2): $(BUILD)/tmasco.o
	ar rvs $@ $^

clean:
	rm -rf $(BUILD)
