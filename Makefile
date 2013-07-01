# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

# -- targets -------------------------------------------------------------------
BUILD   = build


# LIBASCO
SRCS    = heap.c cow.c asco.c
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.o))
LIBASCO = libasco.a

# LIBTMASCO
TMSRCS    = tmasco.c #tmasco_support.c
TMOBJS    = $(addprefix $(BUILD)/, $(TMSRCS:.c=.o))
LIBTMASCO = libtmasco.a

# TESTS
TSRCS = cow_test.c
TESTS = $(addprefix $(BUILD)/, $(TSRCS:.c=.test))

_TARGETS = $(LIBASCO) $(LIBTMASCO)
$(info Targets: $(_TARGETS))
TARGETS = $(addprefix $(BUILD)/, $(_TARGETS))

# -- configuration -------------------------------------------------------------
CFLAGS  = -g -O0 -Wall
#CFLAGS  = -O3

# debugging level 0-3
ifdef ADEBUG
CFLAGS += -DDEBUG=$(ADEBUG)
endif

# ASCO options
AFLAGS = -DTMASCO_ENABLED
ifdef MODE
$(info Compiling asco mode = $(MODE))
ifeq ($(MODE), instr)
AFLAGS += -DMODE=0
endif
ifeq ($(MODE), heap)
AFLAGS += -DMODE=1
endif
ifeq ($(MODE), cow)
AFLAGS += -DMODE=2
endif
else # !MODE
AFLAGS += -DMODE=1
MODE=heap
endif

# compiler
ifndef CC
CC = gcc
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

$(info CFLAGS: $(CFLAGS))
$(info Asco mode: $(MODE))
$(info Compiler: $(CC))
$(info ----------------------)

# -- rules ---------------------------------------------------------------------
.PHONY: all clean test

all: $(TARGETS)

test: $(TESTS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/tmasco_%.o: src/tmasco_%.c $(OBJS)
	$(CC) $(CFLAGS) $(TMFLAGS) -I include -c -o $@ $<

$(BUILD)/%.o : src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(AFLAGS) -I include -c -o $@ $<

$(BUILD)/$(LIBASCO): $(OBJS)
	ar rvs $@ $^

$(BUILD)/$(LIBTMASCO): $(TMOBJS)
	ar rvs $@ $^

$(BUILD)/%.test: src/%.c $(OBJS)
	$(CC) $(CFLAGS) -I include -I src -o $@ $^

clean:
	rm -rf $(BUILD)
