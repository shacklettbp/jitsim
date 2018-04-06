cd llvm

if [ ! -d llvm-6.0.0.src ]; then
  wget "http://releases.llvm.org/6.0.0/llvm-6.0.0.src.tar.xz"
  tar -xf llvm-6.0.0.src.tar.xz
fi

# BUILD LLVM HERE
mkdir build
cd build
cmake -DLLVM_BUILD_LLVM_DYLIB=true -DLLVM_LINK_LLVM_DYLIB=true -DLLVM_TARGETS_TO_BUILD=host -DLLVM_USE_SANITIZER="Address" -DCMAKE_INSTALL_PREFIX=../install -G "Unix Makefiles" ../llvm-6.0.0.src
ASAN_OPTIONS=abort_on_error=0:detect_leaks=0 make -j5
ASAN_OPTIONS=abort_on_error=0:detect_leaks=0 make install

cd ..
