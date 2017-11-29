CXX = g++
CXXFLAGS = -fPIC
CXXFLAGS += -Wall -Werror -pedantic -Wextra
LDFLAGS = -fPIC

ifdef SIMOPT
CXXFLAGS += -O2 -march=native
else
CXXFLAGS += -O0 -ggdb -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all
LDFLAGS += -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all
endif

CXXFLAGS += -Iinclude
LDFLAGS += -Lbuild

CXXFLAGS += -I${HOME}/magma/coreir/include
LDFLAGS += -L${HOME}/magma/coreir/lib

# LLVM
CXXFLAGS += $(shell ./external/llvm/install/bin/llvm-config --cxxflags) -fexceptions -std=c++17
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

depend: build/.depend

build/.depend: $(LIBSRCS) $(BINSRCS)
	rm -f ./build/.depend
	$(CXX) $(CXXFLAGS) -MM $^ | sed -e 's/^\(.\+\.o:\)/build\/objs\/\1/' > ./build/.depend;

include build/.depend

build/objs/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/objs/%.o: binsrc/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/libsimjit.so: $(LIBOBJS)
	$(CXX) $(LDFLAGS) $(LIBOBJS) $(LLVMLDFLAGS) -shared -lcoreir -o $@

build/jitfrontend: build/libsimjit.so $(BINOBJS)
	$(CXX) $(LDFLAGS) $(BINOBJS) $(LLVMLDFLAGS) -Wl,-rpath,build -lsimjit  -o $@

.PHONY: clean
clean:
	rm -rf build/libsimjit.so build/jitfrontend build/objs/*
