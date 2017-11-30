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
    llvm::LLVMContext Context;
    llvm::IRBuilder<> TheBuilder;
    std::map<std::string, llvm::Value *> NamedValues; // not used yet
    // need a way of storing modules maybe?
  public:

    Builder();
    std::unique_ptr<llvm::Module> makeModule(); // no inputs for now

};

} // end namespace JITSim

#endif // JITSIM_JIT_HPP
