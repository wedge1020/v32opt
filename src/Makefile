# --- Toolchain ---
# You can freely change this to: CC = clang
CC = gcc

# --- Flags ---
CFLAGS = -Wall -Wextra -Wno-format-truncation -g
CFLAGS += -I.
CLIBS = -lm

# --- Output Executable ---
TARGET = ../v32opt

# --- Object Files ---

# Core sources
CORE_SRCS = main.c args.c tools.c cfg.c stack.c dce.c cse.c inline.c promote.c helpers.c

# Peephole modules - automatically include all .c files in peephole/
PEEPHOLE_DIR = peephole
PEEPHOLE_SRCS := $(wildcard $(PEEPHOLE_DIR)/*.c)
PEEPHOLE_OBJS := $(PEEPHOLE_SRCS:$(PEEPHOLE_DIR)/%.c=$(PEEPHOLE_DIR)/%.o)

# All sources
SRCS = $(CORE_SRCS) $(PEEPHOLE_SRCS)
OBJS = $(SRCS:.c=.o)

# --- Build Rules ---
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Rule for standard C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean monorule
