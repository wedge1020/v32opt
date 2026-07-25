# --- Base Project Makefile ---

.PHONY: all clean install tests

# Default target: build the main compiler executable inside src/
all:
	$(MAKE) -C src

# Clean both the build files in src/ and the generated assembly in testing/
clean:
	rm -f err.txt v32lua.[ch]
	$(MAKE) -C src clean
	$(MAKE) -C testing clean

# Pass the install target down to the src directory
install:
	$(MAKE) -C src install

# Run the test compilations. 
# We explicitly depend on the optimizer binary ('v32opt') being built first!
tests: v32opt
	$(MAKE) -C testing
