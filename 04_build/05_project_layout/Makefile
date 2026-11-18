CC := arm-linux-gnueabihf-gcc
CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra
LDFLAGS := -static
DEPFLAGS := -MMD -MP

TARGET := build/file_app
OBJECTS := build/main.o build/io_utils.o
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build:
	mkdir -p $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGET)

-include $(DEPS)
