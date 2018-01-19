# Setup
## MacOS

Using System LLVM:
```
brew install llvm
echo 'export PATH="/usr/local/opt/llvm/bin:$PATH"' >> ~/.bashrc
export PATH="/usr/local/opt/llvm/bin:$PATH"
make SYSTEMLLVM=1
```

Using Bundled LLVM (with sanitizers):
```
cd external
./setup_deps.sh
cd ..
make
```

# Test
```
./build/jitfrontend tests/counter.json
```
