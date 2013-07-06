# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

$(info DEBUG: $(DEBUG))

# -- targets -------------------------------------------------------------------
BUILD   = build
SRCS    = heap.c cow.c asco.c tmasco.c tbin.c sinfo.c talloc.c
SUPPORT = tmasco_support.c
LIBASCO = libasco.a

ifdef DEBUG
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.o) $(SUPPORT:.c=.o))
else
OBJS    = $(BUILD)/asco-inline.o $(BUILD)/tmasco_support.o
endif

# TESTS
TSRCS = cow_test.c
TESTS = $(addprefix $(BUILD)/, $(TSRCS:.c=.test))

_TARGETS = $(LIBASCO)
TARGETS = $(addprefix $(BUILD)/, $(_TARGETS))

# -- configuration -------------------------------------------------------------
#CFLAGS_DBG  = -g -O0 -Wall
CFLAGS_DBG  = -g -O0 -Wall -DASCO_STACK_INFO
#CFLAGS_REL  = -g -O1 -Wall # to check inlines
CFLAGS_REL  = -g -O3 #-flto

ifdef DEBUG
override CFLAGS += $(CFLAGS_DBG) -Iinclude
else
override CFLAGS += $(CFLAGS_REL) -Iinclude
endif

# debugging level 0-3
ifdef DEBUG
CFLAGS += -DDEBUG=$(DEBUG)
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
TMFLAGS = -fgnu-tm
ifndef CC
CC = gcc
endif

# pick TM flags for compiler
ifeq ($(CC), clang)
TMFLAGS = -ftm
endif

$(info CFLAGS: $(CFLAGS))
$(info AFLAGS: $(AFLAGS))
$(info Compiler: $(CC))
$(info ----------------------)

# -- rules ---------------------------------------------------------------------
.PHONY: all clean test

all: $(TARGETS)

test: $(TESTS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/tmasco_support.o: src/tmasco_support.c
	$(CC) $(CFLAGS_DBG) $(TMFLAGS) -I include -c -o $@ $<

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
