PREFIX  ?= /usr/local
VERSION := 0.1.0

PKGS    := wayland-client xkbcommon cairo jansson
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -Wall -D_GNU_SOURCE=200809L -DVERSION=\"$(VERSION)\" \
           -Ibuild $(shell pkg-config --cflags $(PKGS))
LDLIBS  += $(shell pkg-config --libs $(PKGS)) -lm

WL_PROTOCOLS    := $(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER := $(shell pkg-config --variable=wayland_scanner wayland-scanner)

PROTOCOLS := \
	$(WL_PROTOCOLS)/stable/viewporter/viewporter.xml \
	$(WL_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml \
	$(WL_PROTOCOLS)/staging/fractional-scale/fractional-scale-v1.xml \
	$(WL_PROTOCOLS)/unstable/xdg-output/xdg-output-unstable-v1.xml \
	protocol/wlr-layer-shell-unstable-v1.xml

PROTO_NAMES   := $(basename $(notdir $(PROTOCOLS)))
PROTO_HEADERS := $(PROTO_NAMES:%=build/%-client-protocol.h)
PROTO_OBJS    := $(PROTO_NAMES:%=build/%-protocol.o)

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:src/%.c=build/%.o) $(PROTO_OBJS)

vpath %.xml $(sort $(dir $(PROTOCOLS)))

build/hypr-resize: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c $(PROTO_HEADERS) | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/%-protocol.o: build/%-protocol.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/%-protocol.c: %.xml | build
	$(WAYLAND_SCANNER) private-code $< $@

build/%-client-protocol.h: %.xml | build
	$(WAYLAND_SCANNER) client-header $< $@

build:
	mkdir -p build

install: build/hypr-resize
	install -Dm755 build/hypr-resize $(DESTDIR)$(PREFIX)/bin/hypr-resize

clean:
	rm -rf build

.PHONY: install clean
.PRECIOUS: build/%-protocol.c
