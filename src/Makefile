# --- Toolchain ---
# You can freely change this to: CC = clang
CC = gcc

# --- Flags ---
CFLAGS = -Wall -Wextra -Wno-format-truncation -g
CFLAGS += -I. -I../inc
# -MMD -MP: each object also writes a .d file listing the headers it
# actually included (pulled in by the -include at the bottom). Without it
# make only compared each .o against its own .c, so editing a header --
# inc/v32opt.h's VERSION, a struct in asm.h -- left stale objects in place
# and `v32opt --version` kept printing the old string.
DEPFLAGS = -MMD -MP
CLIBS = -lm

# --- Output Executable ---
TARGET = ../v32opt

# --- Object Files ---

# Core sources
CORE_SRCS = main.c args.c tools.c cfg.c stack.c dce.c cse.c inline.c promote.c helpers.c defines.c liveness.c

# Peephole modules - automatically include all .c files in peephole/
PEEPHOLE_DIR = peephole
PEEPHOLE_SRCS := $(wildcard $(PEEPHOLE_DIR)/*.c)
PEEPHOLE_OBJS := $(PEEPHOLE_SRCS:$(PEEPHOLE_DIR)/%.c=$(PEEPHOLE_DIR)/%.o)

# All sources
SRCS = $(CORE_SRCS) $(PEEPHOLE_SRCS)
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

# --- Build Rules ---
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(CLIBS)

# Rule for standard C files
%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)

.PHONY: all clean

# Auto-generated header dependencies (see DEPFLAGS above). The leading '-'
# keeps a fresh checkout (no .d files yet) quiet.
-include $(DEPS)
