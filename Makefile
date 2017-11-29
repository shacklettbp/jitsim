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

CXXFLAGS += -Iinclude -Isrc
LDFLAGS += -Lbuild

export CXX
export CFLAGS
export CXXFLAGS
export LDFLAGS

all: build/libsimjit.so build/jitfrontend

BINSRCS =$(wildcard binsrc/[^_]*.cpp)
BINOBJS =$(patsubst binsrc/%.cpp,build/objs/%.o,$(BINSRCS))

LIBSRCS =$(wildcard src/[^_]*.cpp)
LIBOBJS =$(patsubst src/%.cpp,build/objs/%.o,$(LIBSRCS))

build/objs/%.o: src/%.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/objs/%.o: binsrc/%.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/libsimjit.so: $(LIBOBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAG) $< -shared -o $@

build/jitfrontend: build/libsimjit.so $(BINOBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(BINOBJS) -lsimjit -Wl,-rpath,build -o $@

.PHONY: clean
clean:
	rm -rf build/libsimjit.so build/jitfrontend build/objs/*
