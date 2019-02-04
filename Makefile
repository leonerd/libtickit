ifeq ($(shell uname),Darwin)
  LIBTOOL ?= glibtool
else
  LIBTOOL ?= libtool
endif

ifneq ($(VERBOSE),1)
  LIBTOOL +=--quiet
endif

override CFLAGS +=-Wall -Iinclude -Isrc -std=c99

ifeq ($(DEBUG),1)
  override CFLAGS +=-ggdb -DDEBUG
endif

ifeq ($(PROFILE),1)
  override CFLAGS +=-pg
  override LDFLAGS+=-pg
endif

ifeq ($(shell pkg-config --atleast-version=1.1.0 unibilium && echo 1),1)
  override CFLAGS +=$(shell pkg-config --cflags unibilium) -DHAVE_UNIBILIUM
  override LDFLAGS+=$(shell pkg-config --libs   unibilium)
else ifeq ($(shell pkg-config ncursesw && echo 1),1)
  override CFLAGS +=$(shell pkg-config --cflags ncursesw)
  override LDFLAGS+=$(shell pkg-config --libs   ncursesw)
else
  override LDFLAGS+=-lncurses
endif

override CFLAGS +=$(shell pkg-config --cflags termkey)
override LDFLAGS+=$(shell pkg-config --libs   termkey)

CFILES=$(sort $(wildcard src/*.c))
HFILES=$(sort $(wildcard include/*.h))
OBJECTS=$(CFILES:.c=.lo)
LIBRARY=libtickit.la

HFILES_INT=$(sort $(wildcard src/*.h)) $(HFILES)

TESTSOURCES=$(sort $(wildcard t/[0-9]*.c))
TESTFILES=$(TESTSOURCES:.c=.t)

EXAMPLESOURCES=$(sort $(wildcard examples/*.c))

VERSION_CURRENT=1
VERSION_REVISION=0
VERSION_AGE=0

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib
INCDIR=$(PREFIX)/include
MANDIR=$(PREFIX)/share/man
MAN3DIR=$(MANDIR)/man3
MAN7DIR=$(MANDIR)/man7

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(LIBTOOL) --mode=link --tag=CC $(CC) -rpath $(LIBDIR) -version-info $(VERSION_CURRENT):$(VERSION_REVISION):$(VERSION_AGE) -o $@ $^ $(LDFLAGS)

src/%.lo: src/%.c $(HFILES_INT)
	$(LIBTOOL) --mode=compile --tag=CC $(CC) $(CFLAGS) -o $@ -c $<

src/term.lo: src/xterm-palette.inc
src/xterm-palette.inc: src/xterm-palette.inc.PL
	perl $^ > $@

src/renderbuffer.lo: src/linechars.inc
src/linechars.inc: src/linechars.inc.PL
	perl $^ > $@

t/%.t: t/%.c $(LIBRARY) t/taplib.lo t/mockterm.lo t/taplib-tickit.lo
	$(LIBTOOL) --mode=link --tag=CC gcc $(CFLAGS) $(LDFLAGS) -o $@ -Iinclude -std=c99 -ggdb $^

t/%.lo: t/%.c
	$(LIBTOOL) --mode=compile --tag=CC gcc $(CFLAGS) -o $@ -c $^

.PHONY: test
test: $(TESTFILES)
	$(LIBTOOL) --mode=execute prove -e ""

.PHONY: clean-test
clean-test:
	$(LIBTOOL) --mode=clean rm -f $(TESTFILES) t/taplib.lo t/mockterm.lo

.PHONY: clean
clean: clean-test
	$(LIBTOOL) --mode=clean rm -f $(OBJECTS)
	$(LIBTOOL) --mode=clean rm -f $(LIBRARY)

ifneq ($(shell pkg-config glib-2.0 && echo 1),1)
  EXAMPLESOURCES:=$(filter-out examples/evloop-glib.c, $(EXAMPLESOURCES))
endif

examples/evloop-glib.lo: CFLAGS +=$(shell pkg-config --cflags glib-2.0)
examples/evloop-glib:    LDFLAGS+=$(shell pkg-config --libs   glib-2.0)

ifneq ($(shell pkg-config libuv && echo 1),1)
  EXAMPLESOURCES:=$(filter-out examples/evloop-libuv.c, $(EXAMPLESOURCES))
endif

examples/evloop-libuv.lo: CFLAGS +=$(shell pkg-config --cflags libuv)
examples/evloop-libuv:    LDFLAGS+=$(shell pkg-config --libs   libuv)

.PHONY: examples
examples: $(EXAMPLESOURCES:.c=)

examples/%.lo: examples/%.c $(HFILES)
	$(LIBTOOL) --mode=compile --tag=CC $(CC) $(CFLAGS) -o $@ -c $<

examples/%: examples/%.lo $(LIBRARY)
	$(LIBTOOL) --mode=link --tag=CC gcc -o $@ $^ $(LDFLAGS)

.PHONY: install
install: install-inc install-lib install-man
	$(LIBTOOL) --mode=finish $(DESTDIR)$(LIBDIR)

install-inc: $(HFILES)
	install -d $(DESTDIR)$(INCDIR)
	install -m644 $(HFILES) $(DESTDIR)$(INCDIR)
	install -d $(DESTDIR)$(LIBDIR)/pkgconfig
	LIBDIR=$(LIBDIR) INCDIR=$(INCDIR) sh tickit.pc.sh >$(DESTDIR)$(LIBDIR)/pkgconfig/tickit.pc

# rm the old binary first in case it's still in use
install-lib: $(LIBRARY)
	install -d $(DESTDIR)$(LIBDIR)
	$(LIBTOOL) --mode=install install $(LIBRARY) $(DESTDIR)$(LIBDIR)/

install-man:
	install -d $(DESTDIR)$(MAN3DIR)
	install -d $(DESTDIR)$(MAN7DIR)
	for F in man/*.3; do \
	  gzip <$$F >$(DESTDIR)$(MAN3DIR)/$${F#man/}.gz; \
	done
	for F in man/*.7; do \
	  gzip <$$F >$(DESTDIR)$(MAN7DIR)/$${F#man/}.gz; \
	done
	while read FROM EQ TO; do \
	  if [ -n "$$FROM" ]; then \
	    ln -sf $$TO.gz $(DESTDIR)$(MAN3DIR)/$$FROM.gz; \
	  fi; \
	done < man/also

HTMLDIR=html

htmldocs:
	perl $(HOME)/src/perl/Parse-Man/examples/man-to-html.pl -O $(HTMLDIR) --file-extension html --link-extension html --template home_lou.tt2 --also man/also man/*.3 man/*.7 --index html/index.html
