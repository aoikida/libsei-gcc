# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

CFLAGS  = -g -O0 -Wall
CFLAGS  = -O3

ifdef ASCO_DEBUG
CFLAGS += -DDEBUG=$(ASCO_DEBUG)
endif

ASCOFLG = -DASCO_COMPLETE -DTMASCO_ENABLED
BUILD   = build
SRCS    = heap.c cow.c asco.c
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.o))
TARGET  = libasco.a
TARGET2 = libtmasco.a

TMSRCS  = tmasco.c #tmasco_support.c
TMOBJS  = $(addprefix $(BUILD)/, $(TMSRCS:.c=.o))

TSRCS   = cow_test.c
TTARGET = $(addprefix $(BUILD)/, $(TSRCS:.c=.test))

# $(info $(OBJS) $(TTARGET)

# assume gcc as default compiler
ifndef CC
CC=gcc
endif

# pick TM flags for compiler
ifeq ($(CC), gcc)
TMFLAGS = -fgnu-tm
endif
ifeq ($(CC), clang)
TMFLAGS = -ftm
endif

ifeq (,$(TMFLAGS))
$(ERROR unsupported compiler)
endif

.PHONY: all clean test

all: $(BUILD)/$(TARGET) $(BUILD)/$(TARGET2)

test: $(TTARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/tmasco_%.o: src/tmasco_%.c $(OBJS)
	$(CC) $(CFLAGS) $(TMFLAGS) -I include -c -o $@ $<

$(BUILD)/%.o : src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(ASCOFLG) -I include -c -o $@ $<

$(BUILD)/$(TARGET): $(OBJS)
	ar rvs $@ $^

$(BUILD)/$(TARGET2): $(TMOBJS)
	ar rvs $@ $^

$(BUILD)/%.test: src/%.c $(OBJS)
	$(CC) $(CFLAGS) -I include -I src -o $@ $^

clean:
	rm -rf $(BUILD)
