CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?=	-g	-Wall -pthread

TARGET ?=aesdsocket

LDFLAGS ?=	-lpthread -lrt

all:	$(TARGET)

$(TARGET):	$(TARGET)
	$(CC)	$(CFLAGS) -o $(TARGET)	$(TARGET).c

clean:
	rm -f *.o $(TARGET)
