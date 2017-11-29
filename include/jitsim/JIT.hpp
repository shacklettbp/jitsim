#ifndef LLVM_EXECUTIONENGINE_ORC_JIT_H
#define LLVM_EXECUTIONENGINE_ORC_JIT_H

#include <llvm/ADT/STLExtras.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Mangler.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace JITSim {

class JIT {
  private:
    std::unique_ptr<llvm::TargetMachine> TM;
    const llvm::DataLayout DL;
    llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
    llvm::orc::IRCompileLayer<decltype(ObjectLayer), llvm::orc::SimpleCompiler> CompileLayer;

  public:
    using ModuleHandle = decltype(CompileLayer)::ModuleHandleT;

    JIT();

    llvm::TargetMachine &getTargetMachine();

    ModuleHandle addModule(std::unique_ptr<llvm::Module> M);

    llvm::JITSymbol findSymbol(const std::string Name);

    llvm::JITTargetAddress getSymbolAddress(const std::string Name);

    void removeModule(ModuleHandle H);
};

} // end namespace JITSim

#endif // LLVM_EXECUTIONENGINE_ORC_JIT_H
