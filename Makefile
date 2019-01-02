APP = windowchibi

CC = gcc
CFLAGS = -Wall -Wextra -O2
PC_LIBS = imlib2 x11 xrender

PKG_CONFIG = $(shell pkg-config --cflags --libs $(PC_LIBS))

all:
	$(CC) main.c -o $(APP) $(CFLAGS) $(PKG_CONFIG)

clean:
	rm -f $(APP)

