# -----------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# -----------------------------------------------------------------------------

# --- configuration -----------------------------------------------------------
CFLAGS_DBG  = -msse4.2 -g -O0 -Wall 
CFLAGS_REL  = -msse4.2 -g -O3 -Wall -DNDEBUG

# --- default parameters - DO NOT EDIT ----------------------------------------
SEI_2PL=1
SEI_ASMREAD=1 
SEI_APPEND_ONLY=1 
SEI_ROPURE=1 
SEI_WT=1
SEI_MODE=cow
# -----------------------------------------------------------------------------

ifdef DEBUG
override CFLAGS += $(CFLAGS_DBG) -Iinclude
else
override CFLAGS += $(CFLAGS_REL) -Iinclude -D_FORTIFY_SOURCE=0
#-U_FORTIFY_SOURCE
endif

# debugging level 0-3
ifdef DEBUG
override CFLAGS += -DDEBUG=$(DEBUG)
endif

# OS-dependent options
UNAME=$(shell uname -s)

ifeq ($(UNAME),Darwin)
INCS = 
override CFLAGS += -std=gnu89 -arch x86_64 $(INCS)
else
ifeq ($(UNAME),Linux)
override CFLAGS += -frecord-gcc-switches
else
$(error OS not supported)
endif
endif 



# SEI options
AFLAGS = -DTMASCO_ENABLED 
ifdef SEI_MODE
ifeq ($(SEI_MODE), instr)
AFLAGS = -DMODE=0
endif
ifeq ($(SEI_MODE), heap)
AFLAGS += -DMODE=1
TMASCO_NOASM = 1
endif
ifeq ($(SEI_MODE), cow)
 AFLAGS += -DMODE=2
 ifndef SEI_WB
  AFLAGS += -DCOW_WT
 endif
endif
else # !MODE
AFLAGS += -DMODE=2
MODE=cow
endif

ifdef SEI_ASMREAD
AFLAGS += -DCOW_ASMREAD
endif

ifdef SEI_APPEND_ONLY
AFLAGS += -DCOW_APPEND_ONLY
endif

ifdef SEI_ROPURE
AFLAGS += -DCOW_ROPURE
endif

ifdef SEI_MTL
AFLAGS += -DASCO_MTL
endif

ifdef SEI_MTL2
AFLAGS += -DASCO_MTL2
endif

ifdef SEI_2PL
AFLAGS += -DASCO_2PL
endif

ifdef SEI_TBAR
AFLAGS += -DASCO_TBAR
endif

# compiler
ifndef CC
ifeq ($(UNAME),Darwin)
CC = /usr/local/Cellar/gcc/4.9.2/bin/gcc-4.9 
else
CC = gcc
endif
endif

# TM flag for compiler
TMFLAGS = -fgnu-tm

$(info ======================)
$(info UNAME : $(UNAME))
$(info MODE  : $(MODE))
$(info DEBUG : $(DEBUG))
$(info CFLAGS: $(CFLAGS))
$(info AFLAGS: $(AFLAGS))
$(info CC    : $(CC))
$(info ----------------------)

# --- targets -----------------------------------------------------------------
BUILD  ?= build
SRCS    = heap.c cow.c asco.c tmi.c tbin.c sinfo.c talloc.c abuf.c ilog.c \
	cpu_stats.c obuf.c crc.c ibuf.c cfc.c stash.c tbar.c wts.c
SUPPORT = support.c
LIBASCO = libsei.a
OBJS    =

ifndef TMASCO_NOASM
OBJS   += $(BUILD)/tmasco_asm.o
endif

ifdef SEI_ASMREAD
OBJS   += $(BUILD)/tmasco_read.o
endif

ifdef DEBUG
OBJS   += $(addprefix $(BUILD)/, $(SRCS:.c=.o) $(SUPPORT:.c=.o))
else
OBJS   += $(BUILD)/asco-inline.o $(BUILD)/support.o
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

$(BUILD)/tmasco_asm.o: src/tmasco_asm.S | $(BUILD)
	$(CC) $(CFLAGS) -I include -c -o $@ $<

$(BUILD)/support.o: src/support.c
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
