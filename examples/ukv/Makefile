# ------------------------------------------------------------------------------
# Copyright (c) 2013 Diogo Behrens
# Distributed under the MIT license. See accompanying file LICENSE.
# ------------------------------------------------------------------------------

CFLAGS    = -g -O0 -Wall -fno-builtin -fno-omit-frame-pointer 
SFLAGS   := $(CFLAGS)
CFLAGS   += -DSEI_DISABLED 
BUILD     = build
BUILD_SEI = build_sei

# Libraries and paths
INCP = -I../../include 
LIBP = 
LIBS = -lm -ldl

SRCP_SEI = ../../src
INCP_SEI = -I../../include
LIBP_SEI = -L../../build
LIBS_SEI = -lsei $(LIBS)

TMFLAGS = -DNDEBUG -fgnu-tm
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

UKVCLNT = ukv_client.c 

# The following variables keep the complete sources
SRCS     = $(UKVSERV)
OBJS     = $(addprefix $(BUILD)/, $(SRCS:.c=.o))
OBJS_SEI = $(addprefix $(BUILD_SEI)/, $(SRCS:.c=.o))

# $(info $(OBJS))

# there are three main targets: ukv-server, ukv-server.sei and ukv-client.sei
TARGET      = ukv-server
TARGET_SEI  = ukv-server.sei
TARGET_CLNT = ukv-client.sei

# rules
.PHONY: all clean

all: $(BUILD)/$(TARGET) $(BUILD_SEI)/$(TARGET_SEI) $(BUILD_SEI)/$(TARGET_CLNT)

$(BUILD):
	mkdir -p $(BUILD)
	mkdir -p $(BUILD)/hashtable

$(BUILD)/%.o : %.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCP) -c -o $@ $<

$(BUILD)/$(TARGET): $(OBJS)
	$(CC) $(LIBP) -o $@ $^ $(LIBS)

test: $(BUILD)/ukv_test $(BUILD)/ukv_net_test
	$(info "Running tests...")
	$(BUILD)/ukv_test
	$(BUILD)/ukv_net_test

$(BUILD)/ukv_test: ukv_test.c $(addprefix $(BUILD)/, $(UKVSRCS:.c=.o))
	$(CC) $(CFLAGS) $(INCP) $(LIBP) -o $@ $^ $(LIBS)

$(BUILD)/ukv_net_test: ukv_net_test.c $(addprefix $(BUILD)/, $(UKVNSRC:.c=.o))
	$(CC) $(CFLAGS) $(INCP) $(LIBP) -o $@ $^ $(LIBS)

$(BUILD_SEI):
	mkdir -p $(BUILD_SEI)
	mkdir -p $(BUILD_SEI)/hashtable

$(BUILD_SEI)/%.o : %.c   | $(BUILD_SEI)
	$(CC) $(SFLAGS) $(TMFLAGS) $(INCP_SEI) $(INCP) -c -o $@ $<

$(BUILD_SEI)/$(TARGET_SEI): $(OBJS_SEI)
	$(CC) $(LIBP) $(SFLAGS) $(TMFLAGS) $(INCP_SEI) -o $@ $^ $(LIBP_SEI) $(LIBS_SEI)

$(BUILD_SEI)/$(TARGET_CLNT): ukv_client.c 
	$(CC) $(LIBP) $(INCP_SEI) -o $@ $^ $(LIBP_SEI) $(LIBS) -lcrc

clean:
	rm -rf $(BUILD) $(BUILD_SEI)
