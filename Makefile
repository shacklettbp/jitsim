CXX = g++
CXXFLAGS = -std=c++17 -fPIC
CXXFLAGS += -Wall -Werror -pedantic -Wextra
LDFLAGS = -fPIC

ifdef SIMDEBUG
CXXFLAGS += -O0 -ggdb
LDFLAGS += -fsanitize=address -fsanitize=undefined -fsanitize-recover=none
else
CXXFLAGS += -O2 -march=native
endif

CXXFLAGS += -Iinclude
LDFLAGS += -Lbuild

CXXFLAGS += -I${HOME}/magma/coreir/include
LDFLAGS += -L${HOME}/magma/coreir/lib

# LLVM
CXXFLAGS += $(shell ./external/llvm/install/bin/llvm-config --cxxflags) -fexceptions
LLVMLDFLAGS = $(shell ./external/llvm/install/bin/llvm-config --ldflags)
LLVMLDFLAGS += -Wl,-rpath,$(shell ./external/llvm/install/bin/llvm-config --libdir)
LLVMLDFLAGS += $(shell ./external/llvm/install/bin/llvm-config --libs)
export CXX
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
	$(CXX) $(LDFLAGS) $(LIBOBJS) $(LLVMLDFLAGS) -shared -lcoreir -o $@

build/jitfrontend: build/libsimjit.so $(BINOBJS)
	$(CXX) $(LDFLAGS) $(BINOBJS) $(LLVMLDFLAGS) -Wl,-rpath,build -lsimjit  -o $@

.PHONY: clean
clean:
	rm -rf build/libsimjit.so build/jitfrontend build/objs/*
