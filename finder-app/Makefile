CROSS_COMPILE=
CC=$(CROSS_COMPILE)gcc
all: writer

writer: writer.c
	$(CC) -Wall -Werror -o writer writer.c

clean:
	-rm -f *.o writer
