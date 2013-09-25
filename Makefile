# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

# -- targets -------------------------------------------------------------------
BUILD  ?= build
SRCS    = heap.c cow.c asco.c tmasco.c tbin.c sinfo.c talloc.c abuf.c ilog.c\
	cpu_stats.c obuf.c crc.c ibuf.c
SUPPORT = tmasco_support.c
LIBASCO = libasco.a

ifdef DEBUG
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.o) $(SUPPORT:.c=.o))
else
OBJS    = $(BUILD)/asco-inline.o $(BUILD)/tmasco_support.o
endif

ifdef USE_TMASCO_ASM
OBJS   += $(BUILD)/tmasco_asm.o
endif

# TESTS
TSRCS = cow_test.c abuf_test.c obuf_test.c
TESTS = $(addprefix $(BUILD)/, $(TSRCS:.c=.test))

_TARGETS = $(LIBASCO)
TARGETS = $(addprefix $(BUILD)/, $(_TARGETS))

# -- configuration -------------------------------------------------------------
CFLAGS_DBG  = -g -O0 -Wall -Werror #-DASCO_STACK_INFO
CFLAGS_REL  = -g -O3 -Wall -Werror #-DASCO_STATS

ifdef DEBUG
override CFLAGS += $(CFLAGS_DBG) -Iinclude
else
override CFLAGS += $(CFLAGS_REL) -Iinclude
endif

# debugging level 0-3
ifdef DEBUG
override CFLAGS += -DDEBUG=$(DEBUG)
endif

# ASCO options
AFLAGS = -DTMASCO_ENABLED
ifdef MODE
ifeq ($(MODE), instr)
AFLAGS = -DMODE=0
endif
ifeq ($(MODE), heap)
AFLAGS += -DMODE=1
endif
ifeq ($(MODE), cow)
AFLAGS += -DMODE=2 -DCOWBACK
endif
ifeq ($(MODE), fcow)
AFLAGS += -DMODE=4 -DCOWBACK
endif
else # !MODE
AFLAGS += -DMODE=1
MODE=heap
endif

ifdef USE_TMASCO_ASM
AFLAGS += -DTMASCO_ASM
endif


# compiler
TMFLAGS = -fgnu-tm
ifndef CC
CC = gcc
endif

# pick TM flags for compiler
ifeq ($(CC), clang)
TMFLAGS = -ftm
endif

$(info MODE  : $(MODE))
$(info DEBUG : $(DEBUG))
$(info CFLAGS: $(CFLAGS))
$(info AFLAGS: $(AFLAGS))
$(info CC    : $(CC))
$(info ----------------------)

# -- rules ---------------------------------------------------------------------
.PHONY: all clean test

all: $(TARGETS)

test: $(TESTS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/tmasco_asm.o: src/tmasco_asm.S
	$(CC) $(CFLAGS) -I include -c -o $@ $<

$(BUILD)/tmasco_support.o: src/tmasco_support.c
	$(CC) $(CFLAGS) $(AFLAGS) $(TMFLAGS) -I include -c -o $@ $<

$(BUILD)/asco-inline.o: $(addprefix src/, $(SRCS)) | $(BUILD)
	@echo > $(BUILD)/asco-inline.c
	@echo $(foreach f, $^, "#include \"../$(f)\"\n")>> $(BUILD)/asco-inline.c
	$(CC) $(CFLAGS) $(AFLAGS) -I include -c -o $@ $(BUILD)/asco-inline.c

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(AFLAGS) -I include -c -o $@ $<

$(BUILD)/$(LIBASCO): $(OBJS)
	ar rvs $@ $^

$(BUILD)/%.test: src/%.c $(OBJS)
	$(CC) $(CFLAGS) -I include -I src -o $@ $^

clean:
	rm -rf $(BUILD)
