CXX = g++
CXXFLAGS = -fPIC
CXXFLAGS += -Wall -Werror -pedantic -Wextra
LDFLAGS = -fPIC

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
TARGET = so
endif
ifeq ($(UNAME_S),Darwin)
LDFLAGS += -lz -lncurses
TARGET = dylib
endif


ifdef SIMOPT
CXXFLAGS += -O2 -march=native
else
CXXFLAGS += -O0 -g
ifndef NOASAN
CXXFLAGS += -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all
LDFLAGS += -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all
endif
endif

CXXFLAGS += -Iinclude
LDFLAGS += -Lbuild

# FIXME: coreir can be installed into a system path, or this should be added
# explicitly as an option by the user
CXXFLAGS += -isystem ${HOME}/magma/coreir/include
LDFLAGS += -L${HOME}/magma/coreir/lib

# LLVM
ifdef SYSTEMLLVM
CXXFLAGS += $(shell llvm-config --cxxflags) -Wno-unused-parameter -Wno-unknown-warning-option
LLVMLDFLAGS += $(shell llvm-config --ldflags)
LLVMLDFLAGS += $(shell llvm-config --libs)
LLVMLDFLAGS += -Wl,-rpath,$(shell llvm-config --libdir)
else
CXXFLAGS += $(shell ./external/llvm/install/bin/llvm-config --cxxflags) -Wno-unknown-warning-option
LLVMLDFLAGS = $(shell ./external/llvm/install/bin/llvm-config --ldflags)
LLVMLDFLAGS += -Wl,-rpath,$(shell ./external/llvm/install/bin/llvm-config --libdir)
LLVMLDFLAGS += $(shell ./external/llvm/install/bin/llvm-config --libs)
endif

CXXFLAGS += -fexceptions -std=c++14

export CXX
export CXXFLAGS
export LDFLAGS

all: build/libsimjit.$(TARGET) build/jitfrontend

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

build/libsimjit.dylib: $(LIBOBJS)
	$(CXX) $(LDFLAGS) $(LIBOBJS) $(LLVMLDFLAGS) -dynamiclib -lcoreir -o $@

build/jitfrontend: build/libsimjit.so build/objs/jitfrontend.o
	$(CXX) $(LDFLAGS) $(BINOBJS) $(LLVMLDFLAGS) -Wl,-rpath,build -lcoreir -lcoreir-commonlib -lsimjit  -o $@

.PHONY: clean
clean:
	rm -rf build/libsimjit.$(TARGET) build/jitfrontend build/objs/*
