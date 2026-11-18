CC := arm-linux-gnueabihf-gcc
CFLAGS := -std=c11 -Wall -Wextra
LDFLAGS := -static
TARGET := file_app
OBJECTS := main.o io_utils.o

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $(TARGET)

main.o: main.c io_utils.h
	$(CC) $(CFLAGS) -c main.c -o main.o

io_utils.o: io_utils.c io_utils.h
	$(CC) $(CFLAGS) -c io_utils.c -o io_utils.o

clean:
	rm -f $(OBJECTS) $(TARGET)
