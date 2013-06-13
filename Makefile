# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

CFLAGS  = -g -O0 -DDEBUG=0 -Wall
#CFLAGS  = -O3
ASCOFLG = -DASCO_COMPLETE -DTMASCO_ENABLED
BUILD   = build
SRCS    = heap.c cow.c asco.c
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.o))
TARGET  = libasco.a
TARGET2 = libtmasco.a

TMSRCS  = tmasco.c tmasco_support.c
TMOBJS  = $(addprefix $(BUILD)/, $(TMSRCS:.c=.o))

# $(info $(OBJS))

.PHONY: all clean

all: $(BUILD)/$(TARGET) $(BUILD)/$(TARGET2)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/tmasco_%.o: src/tmasco_%.c
	gcc $(CFLAGS) -fgnu-tm -I include -c -o $@ $<

$(BUILD)/%.o : src/%.c | $(BUILD)
	gcc $(CFLAGS) $(ASCOFLG) -I include -c -o $@ $<

$(BUILD)/$(TARGET): $(OBJS)
	ar rvs $@ $^

$(BUILD)/$(TARGET2): $(TMOBJS)
	ar rvs $@ $^

clean:
	rm -rf $(BUILD)
