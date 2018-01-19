CXX = g++
CXXFLAGS = -fPIC
CXXFLAGS += -Wall -Werror -pedantic -Wextra
LDFLAGS = -fPIC

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
TARGET = so
endif
ifeq ($(UNAME_S),Darwin)
CXXFLAGS += -fno-rtti # OSX sanitizers are totally broken when rtti is enabled
LDFLAGS += -lz -lncurses
TARGET = dylib
endif


ifdef SIMOPT
CXXFLAGS += -O2 -march=native
else
CXXFLAGS += -O0 -g
endif

CXXFLAGS += -Iinclude -Wno-unused-parameter -Wno-unknown-warning-option
LDFLAGS += -Lbuild

-include config.mk

# LLVM
LLVM_CONFIG = llvm-config
ifndef SYSTEMLLVM
LLVM_CONFIG = ./external/llvm/install/bin/llvm-config

# Can only use address sanitizer with custom sanitizer build of LLVM
ifndef NOASAN
CXXFLAGS += -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all 
LDFLAGS += -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all
endif

endif

CXXFLAGS += $(shell ${LLVM_CONFIG} --cxxflags) 
LLVMLDFLAGS += $(shell ${LLVM_CONFIG} --ldflags)
LLVMLDFLAGS += $(shell ${LLVM_CONFIG} --libs)
LLVMLDFLAGS += -Wl,-rpath,$(shell ${LLVM_CONFIG} --libdir)

FRONTENDLLVMLDFLAGS = 
ifeq ($(UNAME_S), Linux)
FRONTENDLLVMLDFLAGS = $(LLVMLDFLAGS)
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
	$(CXX) $(LDFLAGS) $(BINOBJS) $(FRONTENDLLVMLDFLAGS) -Wl,-rpath,build -lcoreir -lcoreir-commonlib -lsimjit  -o $@

.PHONY: clean
clean:
	rm -rf build/libsimjit.$(TARGET) build/jitfrontend build/objs/*
