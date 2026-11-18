CC := arm-linux-gnueabihf-gcc
CFLAGS := -std=c11 -Wall -Wextra
LDFLAGS := -static
DEPFLAGS := -MMD -MP
TARGET := file_app
OBJECTS := main.o io_utils.o
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGET)

# 编译器生成的依赖规则与上面的模式规则共同参与构建。
-include $(DEPS)
