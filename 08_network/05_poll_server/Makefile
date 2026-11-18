ARM_CC := arm-linux-gnueabihf-gcc
HOST_CC := gcc
CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra
ARM_LDFLAGS := -static
DEPFLAGS := -MMD -MP

SERVER := build/arm/poll_server
CLIENT := build/host/poll_client
OBJECTS := build/arm/poll_server.o build/host/poll_client.o
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean
all: $(SERVER) $(CLIENT)

$(SERVER): build/arm/poll_server.o
	$(ARM_CC) $(CFLAGS) $(ARM_LDFLAGS) $^ -o $@

$(CLIENT): build/host/poll_client.o
	$(HOST_CC) $(CFLAGS) $^ -o $@

build/arm/%.o: src/%.c | build/arm
	$(ARM_CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/host/%.o: src/%.c | build/host
	$(HOST_CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

build/arm build/host:
	mkdir -p $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(SERVER) $(CLIENT)

-include $(DEPS)
