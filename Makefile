CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

SRC = src/main.cpp src/simulator.cpp src/disasm.cpp
HDR = src/memory.hpp src/decode.hpp src/simulator.hpp

build/rvsim: $(SRC) $(HDR)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRC) -o $@

clean:
	rm -rf build

.PHONY: clean
