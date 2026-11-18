CC := arm-linux-gnueabihf-gcc
CFLAGS := -std=c11 -Wall -Wextra
LDFLAGS := -static
TARGET := file_app
OBJECTS := main.o io_utils.o

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

%.o: %.c io_utils.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
