# Compiler and Flags
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c99
TARGET  := v32opt
SRC     := v32opt.c
OBJ     := $(SRC:.c=.o)

# Default rule
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
