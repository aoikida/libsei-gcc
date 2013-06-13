# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

CFLAGS  = -g -O0 -Wall
BUILD   = build

# there are two main targets: ukv-server and ukv-server.tmasco

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

# The targets
TARGET        = ukv-server
TARGET_TMASCO = ukv-server.tmasco

# Libraries and paths
INCP =
LIBP =
LIBS = -lm

INCP_TMASCO = -I../../include
LIBP_TMASCO = -L../../build
LIBS_TMASCO = -ltmasco $(LIBS)

# TMASCO specific flags
ifeq ($(CC), clang)
TMFLAGS = -DINC_SUPPORT -ftm
else
CC = gcc
TMFLAGS = -DNDEBUG -fgnu-tm
endif

.PHONY: all clean

all: $(BUILD)/$(TARGET) $(BUILD)/$(TARGET_TMASCO)

$(BUILD):
	mkdir -p $(BUILD)
	mkdir -p $(BUILD)/hashtable

$(BUILD)/%.o : %.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCP) -c -o $@ $<

$(BUILD)/$(TARGET): $(OBJS)
	$(CC) $(LIBP) -o $@ $^ $(LIBS)

$(BUILD)/$(TARGET_TMASCO): tmasco_glue.c
	$(CC) $(CFLAGS) $(TMFLAGS) $(INCP_TMASCO) -o $@ $< \
	$(LIBP_TMASCO) $(LIBS_TMASCO)

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

# llvm = False
# #llvm = True

# if llvm:
#     env.Replace(CC = 'clang')
#     env.Append(CFLAGS = '-emit-llvm -DINC_SUPPORT')
#     env.Append(CFLAGS = '-ftm ')
#     env.Replace(OBJSUFFIX = '.bc')
# else:
#     env.Append(CFLAGS = '-fgnu-tm ')
#     env.Append(CFLAGS = '-DNDEBUG ')

# # ukv server (telnet-like) with tmasco
# env.Program('ukv-server.tmasco', ['tmasco_glue.c'], LIBS = ['tmasco', 'm'])
