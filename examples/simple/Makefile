CC        = gcc
CFLAGS    = -g -O0 -Wall
# Flag to enable transactional memory
AFLAGS    = -fgnu-tm
# Comment next line to disable libasco
AFLAGS   += -DTMASCO_ENABLED
LIBS      = -ldl
SRCS      = main.c
# Path to tmasco.h
INCP_ASCO = -I../../include
# Path to the library
LIBP_ASCO = -L../../build
LIBS_ASCO = -lasco $(LIBS)

# Needed for generate_input
INCP_ASCO+= -I../../src
SRCP_ASCO = ../../src

.PHONY: all clean

all: trivial generate_input

trivial: $(SRCS)
	$(CC) $(CFLAGS) $(AFLAGS) $(INCP_ASCO) $(LIBP_ASCO) \
		-o $@ $(SRCS) $(LIBS_ASCO) 

generate_input: generate_input.c $(SRCP_ASCO)/crc.c
	$(CC) -msse4.2 $(INCP_ASCO) -o $@ $^ 

clean:
	rm -f trivial generate_input
