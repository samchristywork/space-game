CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2
LIBS := $(shell pkg-config --libs gl glfw3 glew freetype2)
INCS := $(shell pkg-config --cflags gl glfw3 glew freetype2) $(shell pkg-config --cflags glm 2>/dev/null || echo "-I/usr/include")

SRC := src/main.cpp
BIN := build/main

.PHONY: all clean run

all: $(BIN)

$(BIN): $(SRC) | build
	$(CXX) $(CXXFLAGS) $(INCS) $< -o $@ $(LIBS)

build:
	mkdir -p build

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build
