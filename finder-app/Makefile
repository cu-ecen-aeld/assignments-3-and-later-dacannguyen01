CROSS_COMPILE=
CC=$(CROSS_COMPILE)gcc
all: writer
default:	writer

writer.o:	writer.c
	$(CC) -c writer.c

writer:	writer.o
	$(CC) writer.o -o writer

clean:
	-rm -f *.o writer
