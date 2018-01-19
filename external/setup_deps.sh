cd llvm

if [ ! -d llvm-5.0.1.src ]; then
  wget "http://releases.llvm.org/5.0.1/llvm-5.0.1.src.tar.xz"
  tar -xf llvm-5.0.1.src.tar.xz
fi

# BUILD LLVM HERE
mkdir build
cd build
cmake -DLLVM_BUILD_LLVM_DYLIB=true -DLLVM_LINK_LLVM_DYLIB=true -DLLVM_TARGETS_TO_BUILD=host -DLLVM_USE_SANITIZER="Address" -DCMAKE_INSTALL_PREFIX=../install -G "Unix Makefiles" ../llvm-5.0.1.src
ASAN_OPTIONS=abort_on_error=0:detect_leaks=0 make -j5
ASAN_OPTIONS=abort_on_error=0:detect_leaks=0 make install

cd ..
