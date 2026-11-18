CC := arm-linux-gnueabihf-gcc
CFLAGS := -std=c11 -Wall -Wextra
LDFLAGS := -static
DEPFLAGS := -MMD -MP
TARGET := build/backpressure_demo
OBJECTS := build/backpressure_demo.o
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build:
	mkdir -p $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGET)

-include $(DEPS)
