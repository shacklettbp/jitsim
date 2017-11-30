#ifndef JITSIM_JIT_HPP
#define JITSIM_JIT_HPP

#include <llvm/ADT/STLExtras.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/IRTransformLayer.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Mangler.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace JITSim {

void initializeNativeTarget();

class JIT {
  private:
    std::unique_ptr<llvm::TargetMachine> TM;
    const llvm::DataLayout DL;
    llvm::orc::RTDyldObjectLinkingLayer ObjectLayer;
    llvm::orc::IRCompileLayer<decltype(ObjectLayer), llvm::orc::SimpleCompiler> CompileLayer;
    
    using OptimizeFunction =
        std::function<std::shared_ptr<llvm::Module>(std::shared_ptr<llvm::Module>)>;
    llvm::orc::IRTransformLayer<decltype(CompileLayer), OptimizeFunction> OptimizeLayer;
    std::shared_ptr<llvm::Module> optimizeModule(std::shared_ptr<llvm::Module> M);

    std::unique_ptr<llvm::orc::JITCompileCallbackManager> CompileCallbackManager;
    llvm::orc::CompileOnDemandLayer<decltype(OptimizeLayer)> CODLayer;

  public:
    using ModuleHandle = decltype(CODLayer)::ModuleHandleT;

    JIT();

    llvm::TargetMachine &getTargetMachine();

    ModuleHandle addModule(std::unique_ptr<llvm::Module> M);

    llvm::JITSymbol findSymbol(const std::string Name);

    llvm::JITTargetAddress getSymbolAddress(const std::string Name);

    void removeModule(ModuleHandle H);
};

} // end namespace JITSim

#endif // JITSIM_JIT_HPP
