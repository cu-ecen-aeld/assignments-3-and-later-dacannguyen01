CROSS_COMPILE=

ifeq ($(CC),)
	CC=$(CROSS_COMPILE)gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS= -g -Wall -Werror
endif

ifeq ($(LDFLAGS),)
	LDFLAGS= -lrt -pthread
endif

all: aesdsocket
default: aesdsocket

aesdsocket : aesdsocket.o
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o aesdsocket aesdsocket.o $(LDFLAGS)

aesdsocket.o : aesdsocket.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c aesdsocket.c $(LDFLAGS)

clean :
	rm -f aesdsocket *.o
