CC=gcc
CFLAGS=-Isrc/ -I raylib/build/raylib/include/ -I raygui/src/ -g
LIBS=-L raylib/build/raylib/ -lraylib -lm -lsqlite3

all: build/main

.PHONY: objects
objects: $(patsubst src/%.c, build/%.o, $(wildcard src/*.c))

build/%.o: src/%.c
	mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $@

build/main: objects
	${CC} build/*.o ${LIBS} -o $@

.PHONY: run
run: clean build/main
	export LD_LIBRARY_PATH=raylib/lib/; ./build/main

.PHONY: debug
debug: clean build/main
	export LD_LIBRARY_PATH=raylib/lib/; gdb ./build/main

.PHONY: watch
watch:
	ls src/*.c | entr make run

.PHONY: flamegraph
flamegraph: build/main
	#https://github.com/brendangregg/FlameGraph
	sudo perf record -F 99 -a -g -- ./build/main
	sudo perf script | FlameGraph/stackcollapse-perf.pl | FlameGraph/flamegraph.pl > perf.svg
	firefox perf.svg

.PHONY: clean
clean:
	rm -rf build
