#ifndef JITSIM_BUILDER_H_HPP_INCLUDED
#define JITSIM_BUILDER_H_HPP_INCLUDED

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

namespace JITSim {

class BuilderHardcoded {
  private:
    llvm::LLVMContext context;
    llvm::IRBuilder<> ir_builder;
  public:

    BuilderHardcoded();
    // Makes hard-coded modules
    std::unique_ptr<llvm::Module> makeStructModule();
    std::unique_ptr<llvm::Module> makeExternModule();
    std::unique_ptr<llvm::Module> makeStoreLoadModule();
};

} // end namespace JITSim

#endif // JITSIM_JIT_HPP_INCLUDED
