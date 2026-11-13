# Find all subdirectories containing a Makefile
# This dynamically filters out folders that do not need to be compiled
SUBDIRS := $(dir $(wildcard */Makefile))

# The sub-makes assemble and pack with the stock Vircon32 DevTools. Without them (not in PATH) every
# one of them would fail at its first "assemble"; say so once and skip, instead of ending in an error.
HAVE_DEVTOOLS := $(shell command -v assemble >/dev/null 2>&1 && command -v packrom >/dev/null 2>&1 && echo yes)

.PHONY: all debug clean

# --- Default Target ---
# Runs a standard 'make' in each subdirectory
all:
ifneq ($(HAVE_DEVTOOLS),yes)
	@echo "testing: skipped (assemble and packrom of the stock Vircon32 DevTools are not in PATH)"
else
	@for dir in $(SUBDIRS); do \
		echo "========================================"; \
		echo "Entering directory: $$dir"; \
		echo "========================================"; \
		$(MAKE) -C $$dir; \
	done
endif

# --- Debug Target ---
# Runs 'make debug' in each subdirectory
debug:
ifneq ($(HAVE_DEVTOOLS),yes)
	@echo "testing: skipped (assemble and packrom of the stock Vircon32 DevTools are not in PATH)"
else
	@for dir in $(SUBDIRS); do \
		echo "========================================"; \
		echo "Entering directory (DEBUG): $$dir"; \
		echo "========================================"; \
		$(MAKE) -C $$dir debug; \
	done
endif

# --- Clean Target ---
# Runs 'make clean' in each subdirectory
clean:
	@for dir in $(SUBDIRS); do \
		echo "========================================"; \
		echo "Cleaning directory: $$dir"; \
		echo "========================================"; \
		$(MAKE) -C $$dir clean; \
	done
