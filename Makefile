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
SEI_ROPURE=1 
USE_ABUF = yes
ALGO = sbuf
MODE = cow
# -----------------------------------------------------------------------------


ifdef DEBUG
override CFLAGS += $(CFLAGS_DBG) -Iinclude
else
override CFLAGS += $(CFLAGS_REL) -Iinclude -U_FORTIFY_SOURCE
#-D_FORTIFY_SOURCE=0
# if GCC version > 4.7, disable stack protector
override CFLAGS += -fno-stack-protector
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
AFLAGS = -DSEI_ENABLED 
ifdef MODE
ifeq ($(MODE), instr)
AFLAGS = -DMODE=0
endif
ifeq ($(MODE), heap)
AFLAGS += -DMODE=1
SEI_NOASM = 1
endif
else # !MODE
AFLAGS += -DMODE=2
MODE=cow
endif

ifeq ($(MODE), cow)
 AFLAGS += -DMODE=2
 ifeq ($(ALGO),sbuf)
  AFLAGS += -DCOW_WT
  ifeq ($(USE_ABUF), yes)
   AFLAGS += -DCOW_APPEND_ONLY
  endif
  ifdef SEI_ASMREAD
   AFLAGS += -DCOW_ASMREAD
  endif
  ifdef SEI_ROPURE
   AFLAGS += -DCOW_ROPURE
  else
   AFLAGS += -DSEI_ROSAFE
  endif
endif
ifeq ($(ALGO),clog)
 AFLAGS += -DCOW_WB -DSEI_CLOG
 AFLAGS += -DSEI_ROSAFE
endif
endif

ifdef SEI_MTL
AFLAGS += -DSEI_MTL
endif

ifdef SEI_MTL2
AFLAGS += -DSEI_MTL2
endif

ifdef SEI_2PL
AFLAGS += -DSEI_2PL
endif

ifdef SEI_TBAR
AFLAGS += -DSEI_TBAR
endif

ifdef SEI_CPU_ISOLATION
AFLAGS += -DSEI_CPU_ISOLATION
endif

ifdef SEI_CPU_ISOLATION_MIGRATE_PHASES
AFLAGS += -DSEI_CPU_ISOLATION_MIGRATE_PHASES
endif

ifdef SEI_CRC_MIGRATE_CORES
AFLAGS += -DSEI_CRC_MIGRATE_CORES
endif

ifdef SEI_CRC_REDUNDANCY
AFLAGS += -DSEI_CRC_REDUNDANCY=$(SEI_CRC_REDUNDANCY)
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
$(info ALGO  : $(ALGO))
$(info DEBUG : $(DEBUG))
$(info CFLAGS: $(CFLAGS))
$(info AFLAGS: $(AFLAGS))
$(info CC    : $(CC))
$(info ----------------------)

# --- targets -----------------------------------------------------------------
BUILD  ?= build
SRCS    = heap.c cow.c tbin.c sinfo.c talloc.c abuf.c ilog.c \
	cpu_stats.c obuf.c ibuf.c cfc.c stash.c tbar.c wts.c

ifdef SEI_CPU_ISOLATION
SRCS    += cpu_isolation.c
endif

SUPPORT = support.c crc.c
LIBSEI  = libsei.a
LIBCRC  = libcrc.a
OBJS    =

ifeq ($(MODE),cow)
 SRCS   += mode_cow.c tmi.c
 ifeq ($(ALGO),sbuf)
  ifdef SEI_ASMREAD
    OBJS   += $(BUILD)/tmi_read.o
  endif
 endif
endif
ifeq ($(MODE),heap)
SRCS   += mode_heap.c tmi.c
endif
ifeq ($(MODE),instr)
SRCS    = tmi_mock.c
endif

ifndef SEI_NOASM
OBJS   += $(BUILD)/tmi_asm.o
endif

ifdef DEBUG
OBJS   += $(addprefix $(BUILD)/, $(SRCS:.c=.o) $(SUPPORT:.c=.o))
else
OBJS   += $(BUILD)/inlined.o $(BUILD)/support.o
endif


# TESTS
TSRCS = cow_test.c abuf_test.c obuf_test.c cfc_test.c
TESTS = $(addprefix $(BUILD)/, $(TSRCS:.c=.test))

_TARGETS = $(LIBSEI) $(LIBCRC)
override TARGETS = $(addprefix $(BUILD)/, $(_TARGETS))

# --- rules -------------------------------------------------------------------
.PHONY: all clean test

all: $(TARGETS)

test: $(TESTS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/tmi_asm.o: src/tmi_asm.S | $(BUILD)
	$(CC) $(CFLAGS) -I include -c -o $@ $<

$(BUILD)/tmi_read.o: src/tmi_read.S | $(BUILD)
	$(CC) $(CFLAGS) -I include -c -o $@ $<

$(BUILD)/support.o: src/support.c
	$(CC) $(CFLAGS) $(AFLAGS) $(TMFLAGS) -I include -c -o $@ $<

$(BUILD)/inlined.o: $(addprefix src/, $(SRCS)) | $(BUILD)
	@echo > $(BUILD)/inlined.c
	@echo $(foreach f, $^, "#include \"../$(f)\"\n")>> $(BUILD)/inlined.c
	$(CC) $(CFLAGS) $(AFLAGS) -D_GNU_SOURCE -I include -c -o $@ $(BUILD)/inlined.c

$(BUILD)/%.o: src/%.c 
	$(CC) $(CFLAGS) $(AFLAGS) -I include -c -o $@ $<

$(BUILD)/$(LIBSEI): $(OBJS)
	ar rvs $@ $^

$(BUILD)/%.test: src/%.c $(OBJS)
	$(CC) $(CFLAGS) -I include -I src -o $@ $^

$(BUILD)/crc_pure.o: src/crc.c 
	$(CC) $(CFLAGS) -I include -c -o $@ $<

$(BUILD)/$(LIBCRC): src/crc.c | $(BUILD)/crc_pure.o
	ar rvs $@ $(BUILD)/crc_pure.o

clean:
	rm -rf $(BUILD)
