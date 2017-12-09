# Setup
## MacOS
```
brew install llvm
echo 'export PATH="/usr/local/opt/llvm/bin:$PATH"' >> ~/.zshrc
export PATH="/usr/local/opt/llvm/bin:$PATH"
make SYSTEMLLVM=1
```
