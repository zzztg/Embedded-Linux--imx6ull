CC := arm-linux-gnueabihf-gcc
CFLAGS := -std=c11 -Wall -Wextra
LDFLAGS := -static
DEPFLAGS := -MMD -MP
TARGETS := build/exec_demo build/args_demo
OBJECTS := build/exec_demo.o build/args_demo.o
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean
all: $(TARGETS)

build/exec_demo: build/exec_demo.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/args_demo: build/args_demo.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build:
	mkdir -p $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGETS)

-include $(DEPS)
