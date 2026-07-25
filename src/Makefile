# --- Toolchain ---
# You can freely change this to: CC = clang
CC = gcc

# --- Flags ---
CFLAGS = -Wall -Wextra -g
CLIBS = -lm

# --- Output Executable ---
TARGET = ../v32opt

# --- Object Files ---
# Add runtime_embed.o to your object list
OBJS = stack.o peephole.o promote.o inline.o cfg.o dead.o helpers.o tools.o main.o

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
