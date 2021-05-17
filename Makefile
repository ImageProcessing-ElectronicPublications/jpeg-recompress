PNAME = jpeg-recompress
CC ?= gcc
CFLAGS += -std=c99 -Wall -O3 -I/usr/include
LIBIQA = -liqa
LIBSFRY = -lsmallfry
LIBJPEG = -ljpeg
LDFLAGS += -lm $(LIBJPEG) $(LIBIQA) $(LIBSFRY)
MAKE ?= make
AR ?= ar
RM ?= rm
PROGR = jpeg-recompress
PROGC = jpeg-compare
PROGH = jpeg-hash
PROGZ = jpeg-zfpoint
PROGS = $(PROGR) $(PROGC) $(PROGH) $(PROGZ)
PREFIX ?= /usr/local
INSTALL = install

all: $(PROGS)

jmetrics.a: src/jmetrics.o
	$(AR) crs $@ $^

jpeg-recompress: src/jpeg-recompress.c jmetrics.a
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

jpeg-compare: src/jpeg-compare.c jmetrics.a
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

jpeg-hash: src/jpeg-hash.c jmetrics.a
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

jpeg-zfpoint: src/jpeg-zfpoint.c jmetrics.a
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: test/test.c jmetrics.a
	$(CC) $(CFLAGS) -o test/$@ $^ $(LDFLAGS)
	./test/$@

clean:
	$(RM) -rf $(PROGS) jmetrics.a test/test src/*.o

install: all
	$(INSTALL) -d $(PREFIX)/bin
	$(INSTALL) -m 0755 $(PROGS) $(PREFIX)/bin/

uninstall:
	$(RM) -f $(PREFIX)/bin/$(PROGR) $(PREFIX)/bin/$(PROGC) $(PREFIX)/bin/$(PROGH) $(PREFIX)/bin/$(PROGZ)

.PHONY: test clean install uninstall
