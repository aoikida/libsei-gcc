# -----------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# -----------------------------------------------------------------------------

# --- configuration -----------------------------------------------------------
CFLAGS_DBG  = -msse4.2 -g -O0 -Wall #-Werror
	#-DASCO_STACK_INFO
	#-DASCO_STACK_INFO_CMD=
CFLAGS_REL  = -msse4.2 -g -O3 -Wall -DNDEBUG #-DASCO_STATS
INCS = 
# -Werror
ifdef DEBUG
override CFLAGS += $(CFLAGS_DBG) $(INCS) -arch x86_64 -Iinclude -std=gnu89
else
override CFLAGS += $(CFLAGS_REL) $(INCS) -arch x86_64 -Iinclude -std=gnu89
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
TMASCO_NOASM = 1
endif
ifeq ($(MODE), cow)
 AFLAGS += -DMODE=2
 ifndef COW_WB
  AFLAGS += -DCOW_WT
 endif
endif
ifeq ($(MODE), cor)
AFLAGS += -DMODE=3 -DASCO_MT
endif
else # !MODE
AFLAGS += -DMODE=1
MODE=heap
endif

ifdef COW_ASMREAD
AFLAGS += -DCOW_ASMREAD
endif

ifdef COW_APPEND_ONLY
AFLAGS += -DCOW_APPEND_ONLY
endif

ifdef COW_ROPURE
AFLAGS += -DCOW_ROPURE
endif

ifdef ASCO_MTL
AFLAGS += -DASCO_MTL
endif

ifdef ASCO_MTL2
AFLAGS += -DASCO_MTL2
endif

ifdef ASCO_2PL
AFLAGS += -DASCO_2PL
endif

ifdef ASCO_TBAR
AFLAGS += -DASCO_TBAR
endif

# compiler
TMFLAGS = -fgnu-tm
ifndef CC
CC = /usr/local/Cellar/gcc/4.9.2/bin/gcc-4.9 
endif

# pick TM flags for compiler
ifeq ($(CC), clang)
TMFLAGS = -ftm
endif

$(info ======================)
$(info MODE  : $(MODE))
$(info DEBUG : $(DEBUG))
$(info CFLAGS: $(CFLAGS))
$(info AFLAGS: $(AFLAGS))
$(info CC    : $(CC))
$(info ----------------------)

# --- targets -----------------------------------------------------------------
BUILD  ?= build
SRCS    = heap.c cow.c asco.c tmasco.c tbin.c sinfo.c talloc.c abuf.c ilog.c \
	cpu_stats.c obuf.c crc.c ibuf.c cfc.c stash.c tbar.c wts.c
SUPPORT = tmasco_support.c
LIBASCO = libasco.a

ifdef DEBUG
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.o) $(SUPPORT:.c=.o))
else
OBJS    = $(BUILD)/asco-inline.o $(BUILD)/tmasco_support.o
endif

ifndef TMASCO_NOASM
OBJS   += $(BUILD)/tmasco_asm.o
endif

ifdef COW_ASMREAD
OBJS   += $(BUILD)/tmasco_read.o
endif

# TESTS
TSRCS = cow_test.c abuf_test.c obuf_test.c cfc_test.c
TESTS = $(addprefix $(BUILD)/, $(TSRCS:.c=.test))

_TARGETS = $(LIBASCO)
override TARGETS = $(addprefix $(BUILD)/, $(_TARGETS))

# --- rules -------------------------------------------------------------------
.PHONY: all clean test

all: $(TARGETS)

test: $(TESTS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/tmasco_read.o: src/tmasco_read.S
	$(CC) $(CFLAGS) -I include -c -o $@ $<

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
