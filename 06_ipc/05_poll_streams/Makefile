CC := arm-linux-gnueabihf-gcc
CFLAGS := -std=c11 -Wall -Wextra
LDFLAGS := -static
DEPFLAGS := -MMD -MP
TARGETS := build/poll_capture build/stream_writer
OBJECTS := build/poll_capture.o build/stream_writer.o
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean
all: $(TARGETS)

build/poll_capture: build/poll_capture.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/stream_writer: build/stream_writer.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build:
	mkdir -p $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGETS)

-include $(DEPS)
