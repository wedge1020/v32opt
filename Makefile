# --- Base Project Makefile ---
TARGET = v32opt
ARCH = $(shell uname -m)

.PHONY: all clean install tests

# Default target: build the main compiler executable inside src/
all:
	$(MAKE) -C src

# Clean both the build files in src/ and the generated assembly in testing/
clean:
	rm -f err.txt
	$(MAKE) -C src clean
	$(MAKE) -C testing clean

install: $(TARGET)
	@if [ -d ~/bin/bin.$(ARCH) ]; then \
		echo "Installing $(TARGET) to ~/bin/bin.$(ARCH)/"; \
		install -m 755 $(TARGET) ~/bin/bin.$(ARCH)/$(TARGET); \
	elif [ -d ~/bin ]; then \
		echo "Installing $(TARGET) to ~/bin/"; \
		install -m 755 $(TARGET) ~/bin/$(TARGET); \
	else \
		@echo "Skipping: neither ~/bin/bin.$(ARCH) nor ~/bin exist"; \
	fi

sysinstall: $(TARGET)
	@if [ -d /usr/local/bin ]; then \
		echo "Installing $(TARGET) to /usr/local/bin/"; \
		install -m 755 $(TARGET) /usr/local/bin/$(TARGET); \
	else \
		@echo "Skipping: /usr/local/bin does not exist"; \
	fi

# Run the test compilations. 
# We explicitly depend on the optimizer binary ('v32opt') being built first!
tests: $(TARGET)
	$(MAKE) -C testing
