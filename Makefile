CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra -pedantic -O2 -pthread -Iincludes -I.
BUILD   = build
BIN     = bin

ifeq ($(SIN_BARRERA),1)
    CFLAGS += -DSIN_BARRERA
endif

OBJS_SYNC = $(BUILD)/barrier.o $(BUILD)/rwlock.o $(BUILD)/bqueue.o $(BUILD)/snapshot.o $(BUILD)/state.o
OBJS_CORE = $(BUILD)/fdtd.o $(BUILD)/solver.o $(BUILD)/net_util.o $(BUILD)/protocol.o
OBJS_SERVER = $(BUILD)/commands.o $(BUILD)/job_table.o $(BUILD)/dispatcher.o

all: $(BIN)/fdtd_server $(BIN)/fdtd_client $(BIN)/fdtd_bench

bench: $(BIN)/fdtd_bench

$(BIN)/fdtd_server: $(BUILD)/fdtd_server.o $(OBJS_CORE) $(OBJS_SYNC) $(OBJS_SERVER) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BIN)/fdtd_client: $(BUILD)/fdtd_client.o $(BUILD)/net_util.o | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN)/fdtd_bench: $(BUILD)/fdtd_bench.o $(BUILD)/fdtd.o $(BUILD)/solver.o $(OBJS_SYNC) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ -lm

test:
	@echo "Ejecutando pruebas del proyecto..."
	@if [ -f scripts/run_tests.sh ]; then bash scripts/run_tests.sh; else echo "scripts/run_tests.sh no encontrado."; fi

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD) $(BIN):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(BIN) *.bin

.PHONY: all bench test clean