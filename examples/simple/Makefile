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

.PHONY: all clean

all: simple generate_input

simple: $(SRCS)
	$(CC) $(CFLAGS) $(AFLAGS) $(INCP_ASCO) $(LIBP_ASCO) \
		-o $@ $(SRCS) $(LIBS_ASCO) 

generate_input: generate_input.c
	$(CC) $(INCP_ASCO) $(LIBP_ASCO) -o $@ $^ $(LIBS_ASCO)

clean:
	rm -f simple generate_input
