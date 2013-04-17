# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

CFLAGS = -g -O0
BUILD  = build
SRCS   = heap.c cow.c asco.c tmasco.c
OBJS   = $(addprefix $(BUILD)/, $(SRCS:.c=.o))
TARGET = libasco.a
$(info $(OBJS))

.PHONY: all clean

all: $(BUILD)/$(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o : src/%.c $(BUILD)
	gcc $(CFLAGS) -I include -c -o $@ $<

$(BUILD)/$(TARGET): $(OBJS)
	ar rvs $@ $(OBJS)

clean:
	rm -rf $(BUILD)
