# Setup
## MacOS
```
brew install llvm
echo 'export PATH="/usr/local/opt/llvm/bin:$PATH"' >> ~/.zshrc
export PATH="/usr/local/opt/llvm/bin:$PATH"
make SYSTEMLLVM=1
# NOTE: There were issues with apple's default g++, using brew's clang got us
# further, use CXX to try this
make CXX=clang++ SYSTEMLLVM=1
```
