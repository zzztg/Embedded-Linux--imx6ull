CC := arm-linux-gnueabihf-gcc
CFLAGS := -std=c11 -Wall -Wextra
THREADFLAGS := -pthread
LDFLAGS := -static
DEPFLAGS := -MMD -MP
TARGET := build/close_demo
OBJECTS := build/close_demo.o
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(THREADFLAGS) $(LDFLAGS) $^ -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) $(THREADFLAGS) $(DEPFLAGS) -c $< -o $@

build:
	mkdir -p $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGET)

-include $(DEPS)
