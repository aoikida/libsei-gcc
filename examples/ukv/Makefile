# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

CFLAGS  = -g -O0 -Wall -fno-builtin -fno-omit-frame-pointer
BUILD   = build

# Libraries and paths
INCP =
LIBP =
LIBS = -lm -ldl

INCP_ASCO = -I../../include
LIBP_ASCO = -L../../build
LIBS_ASCO = -lasco $(LIBS)

TMFLAGS = -DNDEBUG -fgnu-tm -DTMASCO_ENABLED
CC = gcc

# OS-dependent options
UNAME = $(shell uname -s)
ifeq ($(UNAME),Darwin)
	CFLAGS += -v -Wl,-t -fno-builtin -D_FORTIFY_SOURCE=0
	LIBS += -lcrt1.o -lc
	CC = $(GCC)
#INCP = -I/usr/local/Cellar/gcc/4.9.2/lib/gcc/x86_64-apple-darwin14.0.0/4.9.2/include/c++/tr1/ -I/usr/local/Cellar/gcc/4.9.2/lib/gcc/x86_64-apple-darwin14.0.0/4.9.2/include/c++/
endif

# UKVSRCS are the sources for the ukv core functionality
UKVSRCS = ukv.c \
	hashtable/hashtable.c \
	hashtable/hashtable_utility.c \
	hashtable/hashtable_itr.c

# UKVNSRC are the sources for the network parsing and execution
UKVNSRC = ukv_net.c $(UKVSRCS)

# UKVSERV are the sources for the real networking
UKVSERV = ukv_server.c $(UKVNSRC)

# The following variables keep the complete sources
SRCS    = $(UKVSERV)
OBJS    = $(addprefix $(BUILD)/, $(SRCS:.c=.o))

# $(info $(OBJS))

# there are two main targets: ukv-server and ukv-server.tmasco
TARGET      = ukv-server
TARGET_ASCO = ukv-server.asco

# rules
.PHONY: all clean

all: $(BUILD)/$(TARGET) $(BUILD)/$(TARGET_ASCO)

$(BUILD):
	mkdir -p $(BUILD)
	mkdir -p $(BUILD)/hashtable

$(BUILD)/%.o : %.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCP) -c -o $@ $<

$(BUILD)/$(TARGET): $(OBJS)
	$(CC) $(LIBP) -o $@ $^ $(LIBS)

$(BUILD)/$(TARGET_ASCO): tmasco_glue.c
	$(CC) $(CFLAGS) $(TMFLAGS) $(INCP_ASCO) -o $@ $< \
	$(LIBP_ASCO) $(LIBS_ASCO)

clean:
	rm -rf $(BUILD)

test: $(BUILD)/ukv_test $(BUILD)/ukv_net_test
	$(info "Running tests...")
	$(BUILD)/ukv_test
	$(BUILD)/ukv_net_test

$(BUILD)/ukv_test: ukv_test.c $(addprefix $(BUILD)/, $(UKVSRCS:.c=.o))
	$(CC) $(CFLAGS) $(INCP) $(LIBP) -o $@ $^ $(LIBS)

$(BUILD)/ukv_net_test: ukv_net_test.c $(addprefix $(BUILD)/, $(UKVNSRC:.c=.o))
	$(CC) $(CFLAGS) $(INCP) $(LIBP) -o $@ $^ $(LIBS)

