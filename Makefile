CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2
LIBS := $(shell pkg-config --libs gl glfw3 glew)
INCS := $(shell pkg-config --cflags gl glfw3 glew) $(shell pkg-config --cflags glm 2>/dev/null || echo "-I/usr/include")

SRC := src/main.cpp
BIN := build/main

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC) | build
	$(CXX) $(CXXFLAGS) $(INCS) $< -o $@ $(LIBS)

build:
	mkdir -p build

clean:
	rm -rf build
