CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra -pedantic -O2 -pthread
BUILD   = build
BIN     = bin

all: $(BIN)/fdtd_server $(BIN)/fdtd_client

$(BIN)/fdtd_server: $(BUILD)/fdtd_server.o $(BUILD)/net_util.o | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/fdtd_client: $(BUILD)/fdtd_client.o $(BUILD)/net_util.o | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: src/%.c includes/net_util.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD) $(BIN):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(BIN)

.PHONY: all clean