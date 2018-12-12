CC ?= gcc
CFLAGS += -std=c99 -Wall -O3
LDFLAGS += -lm
MAKE ?= make
PREFIX ?= /usr/local
INSTALL = install
LIBIQA = -liqa
LIBSFRY = -lsmallfry

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
	# Linux (e.g. Debian)
	CFLAGS += -I/usr/include/mozjpeg
	LIBJPEG = -lmozjpeg
else ifeq ($(UNAME_S),Darwin)
	# Mac OS X
	MOZJPEG_PREFIX ?= /usr/local/opt/mozjpeg
	LIBJPEG = $(MOZJPEG_PREFIX)/lib/libjpeg.a
	CFLAGS += -I$(MOZJPEG_PREFIX)/include
else ifeq ($(UNAME_S),FreeBSD)
	# FreeBSD
	LIBJPEG = $(PREFIX)/lib/mozjpeg/libjpeg.so
	CFLAGS += -I$(PREFIX)/include/mozjpeg
else
	# Windows
	LIBJPEG = ../mozjpeg/libjpeg.a
	CFLAGS += -I../mozjpeg
endif

all: jpeg-recompress jpeg-compare jpeg-hash jpeg-zfpoint

jpeg-recompress: src/jpeg-recompress.c src/util.o src/edit.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBJPEG) $(LIBIQA) $(LIBSFRY) $(LDFLAGS)

jpeg-compare: src/jpeg-compare.c src/util.o src/hash.o src/edit.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBJPEG) $(LIBIQA) $(LIBSFRY) $(LDFLAGS)

jpeg-hash: src/jpeg-hash.c src/util.o src/hash.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBJPEG) $(LDFLAGS)

jpeg-zfpoint: src/jpeg-zfpoint.c src/util.o src/edit.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBJPEG) $(LIBIQA) $(LIBSFRY) $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: test/test.c src/util.o src/edit.o src/hash.o
	$(CC) $(CFLAGS) -o test/$@ $^ $(LIBJPEG) $(LDFLAGS)
	./test/$@

install: all
	$(INSTALL) -d $(PREFIX)/bin
	$(INSTALL) -m 0755 jpeg-recompress $(PREFIX)/bin/
	$(INSTALL) -m 0755 jpeg-compare $(PREFIX)/bin/
	$(INSTALL) -m 0755 jpeg-hash $(PREFIX)/bin/
	$(INSTALL) -m 0755 jpeg-zfpoint $(PREFIX)/bin/

clean:
	rm -rf jpeg-recompress jpeg-compare jpeg-hash jpeg-zfpoint test/test src/*.o

.PHONY: test install clean
