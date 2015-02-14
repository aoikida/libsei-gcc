CC        = gcc
CFLAGS    = -g -O0 -Wall
# Flag to enable transactional memory
AFLAGS    = -fgnu-tm
# Comment next line to disable libsei
AFLAGS   += -DTMASCO_ENABLED
LIBS      = -ldl
SRCS      = main.c
# Path to sei.h
INCP_SEI  = -I../../include
# Path to the library
LIBP_SEI  = -L../../build
LIBS_SEI  = -lasco $(LIBS)

.PHONY: all clean

all: simple generate_input

simple: $(SRCS)
	$(CC) $(CFLAGS) $(AFLAGS) $(INCP_SEI) $(LIBP_SEI) \
		-o $@ $(SRCS) $(LIBS_SEI) 

generate_input: generate_input.c
	$(CC) $(INCP_SEI) $(LIBP_SEI) -o $@ $^ $(LIBS_SEI)

clean:
	rm -f simple generate_input
