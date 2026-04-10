CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -O2 -Iinclude
OBJS=src/opcodes.o src/registers.o src/memory.o src/vm.o

all: flux-runtime flux-asm test_vm test_memory test_asm

flux-runtime: src/main.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

flux-asm: src/asm.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

test_vm: tests/test_vm.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

test_memory: tests/test_memory.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

test_asm: tests/test_asm.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: test_vm test_memory test_asm
	@echo "--- VM Tests ---"
	@./test_vm
	@echo "--- Memory Tests ---"
	@./test_memory
	@echo "--- Assembler Tests ---"
	@./test_asm

clean:
	rm -f flux-runtime flux-asm test_vm test_memory test_asm src/*.o

.PHONY: all test clean

isa_v2: src/isa_v2.c src/isa_v2.h
	gcc -Wall -O2 -o test_isa_v2 tests/test_isa_v2.c src/isa_v2.c -I src

test_isa_v2: isa_v2
	./test_isa_v2
