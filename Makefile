# --- Base Project Makefile ---
TARGET = v32opt
ARCH = $(shell uname -m)

# Install locations for `make sysinstall` (override on the command line,
# e.g. `make sysinstall PREFIX=/opt/v32opt`). Same layout the CMake
# build installs by default on Linux/macOS.
PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin
MANDIR   = $(PREFIX)/share/man/man1

# Language of the installed manual page: en (the default), es (Spanish)
# or fr (French), e.g. `sudo make sysinstall DEST_LANG=es`. The translated
# page (man/v32opt.es.1, man/v32opt.fr.1) is installed under the plain
# name v32opt.1, so `man v32opt` shows it. The CMake build takes the same
# setting (-DDEST_LANG=es) and also installs the matching README there.
LANGS     = en es fr
DOC_LANG := $(if $(strip $(DEST_LANG)),$(strip $(DEST_LANG)),en)
ifeq ($(filter $(DOC_LANG),$(LANGS)),)
$(error DEST_LANG must be one of: $(LANGS) (got '$(DEST_LANG)'))
endif
ifeq ($(DOC_LANG),en)
MANPAGE   = man/v32opt.1
else
MANPAGE   = man/v32opt.$(DOC_LANG).1
endif

# The optimizer version: inc/v32opt.h's VERSION is the single source.
# `v32opt --version` reads it from the header directly, and so does the
# CMake build (at configure time). `make version` shows it and stamps it
# into the other file that prints it (the man page's .TH header).
#
# To release, either edit the #define in inc/v32opt.h and run
#     make version
# or let make do the edit too:
#     make version VERSION=20261015-release
# Then rebuild (`make`) so the binary picks up the new string.
#
# Portable between GNU and BSD/macOS tools: no `sed -i` (BSD sed takes the
# script as the backup suffix), no \t in brackets, no `date +%-d`.
VERSION := $(shell sed -n 's/^\#define[[:space:]]*VERSION[[:space:]]*"\(.*\)".*/\1/p' inc/v32opt.h)
MONTH   := $(shell LC_ALL=C date +"%B %Y")
# The translated man pages get the month in their own language (spelled
# out here rather than through locales, which may not be installed).
MONTH_N := $(shell date +%m | sed 's/^0//')
YEAR    := $(shell date +%Y)
MONTHS_ES = enero febrero marzo abril mayo junio julio agosto septiembre octubre noviembre diciembre
MONTHS_FR = janvier février mars avril mai juin juillet août septembre octobre novembre décembre
MONTH_ES := $(word $(MONTH_N),$(MONTHS_ES)) de $(YEAR)
MONTH_FR := $(word $(MONTH_N),$(MONTHS_FR)) $(YEAR)

# Recurse into src/ every time: its own Makefile decides what (if
# anything) is out of date. As a file-based target this used to be
# considered up to date whenever an old bin/v32opt existed, so `make
# install` could install a stale binary -- and with no bin/v32opt at all
# it failed with "No rule to make target".
.PHONY: all clean distclean install uninstall sysinstall sysuninstall \
        tests version monofiles archive put bin/$(TARGET)

# Default target: build the optimizer executable (src/Makefile writes it
# to bin/v32opt, or bin/v32opt.exe on Windows)
all: bin/$(TARGET)

bin/$(TARGET):
	$(MAKE) -C src

# Clean both the build files in src/ and the generated assembly in testing/
clean:
	rm -f err.txt put/* *.zip ../v32opt-project.zip
	$(MAKE) -C src clean
	$(MAKE) -C testing clean
	-rmdir bin 2>/dev/null

# clean, plus the out-of-tree CMake build directory (see CMakeLists.txt)
distclean: clean
	rm -rf build

install: bin/$(TARGET)
	@if [ -d ~/bin/bin.$(ARCH) ]; then \
		echo "Installing $(TARGET) to ~/bin/bin.$(ARCH)/"; \
		install -m 755 bin/$(TARGET) ~/bin/bin.$(ARCH)/$(TARGET); \
	elif [ -d ~/bin ]; then \
		echo "Installing $(TARGET) to ~/bin/"; \
		install -m 755 bin/$(TARGET) ~/bin/$(TARGET); \
	else \
		echo "Skipping: neither ~/bin/bin.$(ARCH) nor ~/bin exist"; \
	fi

uninstall:
	rm -f ~/bin/bin.$(ARCH)/$(TARGET) ~/bin/$(TARGET)

# System-wide install: the binary to $(BINDIR), the manual page (in
# DEST_LANG's language) to $(MANDIR). May need sudo.
sysinstall: bin/$(TARGET)
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)
	install -m 755 bin/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -m 644 $(MANPAGE) $(DESTDIR)$(MANDIR)/v32opt.1
	@echo "Installed $(TARGET) to $(BINDIR) and its manual page ($(DOC_LANG)) to $(MANDIR)"

sysuninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET) $(DESTDIR)$(MANDIR)/v32opt.1

# Show the version (from inc/v32opt.h) and stamp it into the man pages
# (English, Spanish, French). With VERSION=... on the command line, write
# that into inc/v32opt.h first. Run before a release, then rebuild.
version:
	@sed 's/^\(#define[[:space:]]*VERSION[[:space:]]*\)"[^"]*"/\1"$(VERSION)"/' inc/v32opt.h > inc/v32opt.h.tmp && \
	    if cmp -s inc/v32opt.h inc/v32opt.h.tmp; then rm -f inc/v32opt.h.tmp; \
	    else mv inc/v32opt.h.tmp inc/v32opt.h; fi
	@echo "v32opt $(VERSION)"
	@sed 's/^\.TH V32OPT 1 "[^"]*" "v32opt [^"]*"/.TH V32OPT 1 "$(MONTH)" "v32opt $(VERSION)"/' man/v32opt.1 > man/v32opt.1.tmp && mv man/v32opt.1.tmp man/v32opt.1
	@sed 's/^\.TH V32OPT 1 "[^"]*" "v32opt [^"]*"/.TH V32OPT 1 "$(MONTH_ES)" "v32opt $(VERSION)"/' man/v32opt.es.1 > man/v32opt.es.1.tmp && mv man/v32opt.es.1.tmp man/v32opt.es.1
	@sed 's/^\.TH V32OPT 1 "[^"]*" "v32opt [^"]*"/.TH V32OPT 1 "$(MONTH_FR)" "v32opt $(VERSION)"/' man/v32opt.fr.1 > man/v32opt.fr.1.tmp && mv man/v32opt.fr.1.tmp man/v32opt.fr.1
	@grep -H '^#define[[:space:]]*VERSION' inc/v32opt.h
	@grep -H "^\.TH" man/v32opt.1 man/v32opt.es.1 man/v32opt.fr.1

monofiles:
	@mkdir -p put
	@rm -f put/*
	scripts/monolithic_code.sh

# Run the test compilations.
# We explicitly depend on the optimizer binary ('v32opt') being built first!
tests: bin/$(TARGET)
	$(MAKE) -C testing
	bash testing/long-lines/run.sh bin/$(TARGET)
	bash testing/promote/run.sh bin/$(TARGET)
	bash testing/loop-invert/run.sh bin/$(TARGET)
	bash testing/loop-unroll/run.sh bin/$(TARGET)
	bash testing/loop-index/run.sh bin/$(TARGET)
	bash testing/loop-unroll-partial/run.sh bin/$(TARGET)
	bash testing/profile-loops/run.sh bin/$(TARGET)
	bash testing/lint/run.sh bin/$(TARGET)

archive: clean
	zip -r ../v32opt-project.zip .github doc ISSUES Makefile CMakeLists.txt cmake man README.md README.es.md README.fr.md inc scripts src testing

put: clean
	@mkdir -p put
	@rm -f put/*
	@cp inc/*.h src/*.c README.md README.es.md README.fr.md put/
	@cp src/peephole/*.c put/
	@cp man/v32opt.1  put/v32opt.1.txt
	@cp man/v32opt.es.1 put/v32opt.es.1.txt
	@cp man/v32opt.fr.1 put/v32opt.fr.1.txt
	@cp Makefile      put/base_Makefile.txt
	@cp CMakeLists.txt put/CMakeLists.txt
	@cp doc/*         put/
	@cp ISSUES        put/ISSUES.txt
	@cp src/Makefile  put/src_Makefile.txt
