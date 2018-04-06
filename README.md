# Setup
## LLVM
### Bundled
This builds and installs a debug version of LLVM in external/install, which
is necessary for debugging code generation and using ASAN and UBSAN in the
overall project.
```
cd external
./setup_deps.sh
cd ..
```

### Linux System Install
System install of LLVM 6.0.x is required

### MacOS System Install
Apple's bundled LLVM is incomplete, use homebrew:
```
brew install llvm
echo 'export PATH="/usr/local/opt/llvm/bin:$PATH"' >> ~/.bashrc
export PATH="/usr/local/opt/llvm/bin:$PATH"
```

# Build
Use bundled LLVM:
```
make
```

Use system LLVM:
```
make SYSTEMLLVM=1
```

Optimized Build:
```
make SYSTEMLLVM=1 SIMOPT=1
```

# Test
```
./build/jitfrontend tests/counter.json
```
