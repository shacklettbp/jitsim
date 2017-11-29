CXX = g++
CFLAGS = -Wall -fPIC -Werror
CXXFLAGS = -std=c++17 -Wall -fPIC -Werror
LDFLAGS = -fPIC

ifdef SIMDEBUG
CXXFLAGS += -O0 -ggdb
LDFLAGS += -fsanitize=address -fsanitize=undefined -fsanitize-recover=none
else
CXXFLAGS += -O3
endif

export CXX
export CFLAGS
export CXXFLAGS
export LDFLAGS

all: build/simjit.so

SRCS =$(wildcard src/[^_]*.cpp)
OBJS =$(patsubst src/%.cpp,build/%.o,$(SRCS))

build/%.o: src/%.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/simjit.so: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -shared -o $@

.PHONY: clean
clean:
	rm -rf build/*
