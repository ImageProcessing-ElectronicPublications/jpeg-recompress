PNAME = jpeg-recompress
CC ?= gcc
CFLAGS += -std=c99 -Wall -O3
LIBIQA = -liqa
LIBSFRY = -lsmallfry
LIBJPEG = -ljpeg
LIBWEBP = -lwebp
LIBIMM = jmetrics.a
LDFLAGS += -lm $(LIBJPEG) $(LIBIQA) $(LIBSFRY)
PROGR = jpeg-recompress
PROGC = jpeg-compare
PROGH = jpeg-hash
PROGZ = jpeg-zfpoint
PROGW = webp-compress
PROGS = $(PROGR) $(PROGC) $(PROGH) $(PROGZ) $(PROGW)
PREFIX ?= /usr/local
MAKE ?= make
AR ?= ar
RM ?= rm
INSTALL = install

.PHONY: test clean install uninstall

all: $(PROGS)

$(LIBIMM): src/jmetrics.o
	$(AR) crs $@ $^

$(PROGR): src/$(PROGR).c $(LIBIMM)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(PROGC): src/$(PROGC).c $(LIBIMM)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(PROGH): src/$(PROGH).c $(LIBIMM)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(PROGZ): src/$(PROGZ).c $(LIBIMM)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(PROGW): src/$(PROGW).c $(LIBIMM)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBWEBP)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: test/test.c $(LIBIMM)
	$(CC) $(CFLAGS) -o test/$@ $^ $(LDFLAGS)
	./test/$@

clean:
	$(RM) -rf $(PROGS) $(LIBIMM) test/test src/*.o

install: all
	$(INSTALL) -d $(PREFIX)/bin
	$(INSTALL) -m 0755 $(PROGS) $(PREFIX)/bin/

uninstall:
	$(RM) -f $(PREFIX)/bin/$(PROGR) $(PREFIX)/bin/$(PROGC) $(PREFIX)/bin/$(PROGH) $(PREFIX)/bin/$(PROGZ)
