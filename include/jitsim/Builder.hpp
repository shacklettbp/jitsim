#ifndef JITSIM_BUILDER_HPP
#define JITSIM_BUILDER_HPP

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

namespace JITSim {

class Builder {
  private:
    llvm::LLVMContext context;
    llvm::IRBuilder<> ir_builder;
    //std::map<std::string, llvm::Value *> NamedValues; // Not yet used.
  public:

    Builder();
    // Makes hard-coded module
    std::unique_ptr<llvm::Module> makeExternModule();
    std::unique_ptr<llvm::Module> makeStructModule();
    int adder(int lhs, int rhs);
};

} // end namespace JITSim

#endif // JITSIM_JIT_HPP
