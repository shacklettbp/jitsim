cd llvm

if [ ! -d llvm-5.0.0.src ]; then
  wget "http://releases.llvm.org/5.0.0/llvm-5.0.0.src.tar.xz"
  tar -xf llvm-5.0.0.src.tar.xz
fi

# BUILD LLVM HERE
mkdir build
cd build
cmake -DLLVM_BUILD_LLVM_DYLIB=true -DLLVM_LINK_LLVM_DYLIB=true -DCMAKE_INSTALL_PREFIX=../install -G "Unix Makefiles" ../llvm-5.0.0.src
make -j5
make install

cd ..
