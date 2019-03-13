CFLAGS=-Wall -pedantic -O3 -Os -fno-strict-aliasing -std=gnu99
LIBS=-lX11 -lXi

all: corpux-keylogger
	chmod +x corpux-analyzer

corpux-keylogger: corpux-keylogger.c
	$(CC) $^ $(CFLAGS) $(LIBS) -o $@

