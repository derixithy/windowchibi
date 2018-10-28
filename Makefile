APP = windowchibi

CC = gcc
CFLAGS = -Wall -Wextra -O2
PC_LIBS = imlib2 x11 xrender

PKG_CONFIG = $(shell pkg-config --cflags --libs $(PC_LIBS))

all:
	$(CC) $(CFLAGS) $(PKG_CONFIG) main.c -o $(APP)

clean:
	rm -f $(APP)

