project_name := jpeg-recompress
project_data := man src test CHANGELOG Dependencies LICENSE Makefile meson* plot_um.svg README.md

.SILENT:
.PHONY: all dist dist-clean install clean help update-version clear-version configure build

all: configure build

install:
	cd build && \
	meson install && \
	:

clean:
	rm -rf build && \
	:

help:
	echo "Please provide a target:" ; \
	echo "  [default]   - configure and build" ; \
	echo "   install    - install to system" ; \
	echo "   clean      - delete build dir" ; \
	:

update-version:
	version=$(shell git describe --tags --abbrev=7 | sed -E 's/^[^0-9]*//;s/-([0-9]*-g.*)$$/.r\1/;s/-/./g') && \
	meson rewrite kwargs set project / version $$version && \
	echo "project version: $$version" && \
	:

clear-version:
	meson rewrite kwargs delete project / version - && \
	:

configure:
	meson setup build && \
	:

build:
	export PKG_CONFIG_PATH="$(pkgconf_path)" && \
	cd build && \
	ninja && \
	:

dist:
	export version=$(shell git describe --tags --abbrev=7 | sed -E 's/^[^0-9]*//;s/-([0-9]*-g.*)$$/.r\1/;s/-/./g') && \
	echo "... $${version} ..." && \
	export pkgdir="$(project_name)_$$version" && \
	echo "... $${pkgdir} ..." && \
	mkdir -p "$${pkgdir}" && \
	cp --reflink=auto -r -t "$${pkgdir}/" -- $(project_data) && \
	pushd "$${pkgdir}" > /dev/null && \
	echo "... rewrite version..." && \
	meson rewrite kwargs set project / version $$version && \
	popd > /dev/null && \
	echo "... creating tarball..." && \
	tar -c -J --numeric-owner --owner=0 -f "$${pkgdir}.tar.xz" "$${pkgdir}" && \
	:

dist-clean:
	rm -rf $(project_name)_*/ && \
	rm -f $(project_name)_*.tar.xz && \
	:
