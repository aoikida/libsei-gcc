
CC        = gcc
CFLAGS    = -g -O0 -Wall
CFLAGS +  = -fno-builtin -D_FORTIFY_SOURCE = 0 -fno-omit-frame-pointer
AFLAGS    = -fgnu-tm
# Comment next line to disable libasco
AFLAGS +  = -DTMASCO_ENABLED
LIBS      = -lm -ldl
SRCS      = foo.c simple.c
INCP_ASCO = -I../../include
LIBP_ASCO = -L../../build
LIBS_ASCO = -lasco $(LIBS)

simple: $(SRCS)
	$(CC) $(CFLAGS) $(AFLAGS) $(INCP_ASCO) $(LIBP_ASCO) \
		-o $@ $(SRCS) $(LIBS_ASCO) 

clean:
	rm -rf simple
