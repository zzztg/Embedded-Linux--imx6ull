CC := arm-linux-gnueabihf-gcc
CFLAGS := -std=c11 -Wall -Wextra
LDFLAGS := -static
DEPFLAGS := -MMD -MP
TARGETS := build/dup_demo build/redirect_demo
OBJECTS := build/dup_demo.o build/redirect_demo.o
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean
all: $(TARGETS)

build/dup_demo: build/dup_demo.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/redirect_demo: build/redirect_demo.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build:
	mkdir -p $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGETS)

-include $(DEPS)
