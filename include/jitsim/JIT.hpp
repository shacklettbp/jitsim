#ifndef JITSIM_JIT_HPP_INCLUDED
#define JITSIM_JIT_HPP_INCLUDED

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

class JIT {
private:
  const llvm::DataLayout data_layout;

  llvm::orc::RTDyldObjectLinkingLayer object_layer;
  llvm::orc::IRCompileLayer<decltype(object_layer), llvm::orc::SimpleCompiler> compile_layer;
  
  using OptimizeFunction =
      std::function<std::shared_ptr<llvm::Module>(std::shared_ptr<llvm::Module>)>;
  llvm::orc::IRTransformLayer<decltype(compile_layer), OptimizeFunction> optimize_layer;
  std::shared_ptr<llvm::Module> optimizeModule(std::shared_ptr<llvm::Module> module);

  std::unique_ptr<llvm::orc::JITCompileCallbackManager> compile_callback_manager;
  llvm::orc::CompileOnDemandLayer<decltype(optimize_layer)> cod_layer;

  std::string mangle(const std::string name);

public:
  using ModuleHandle = decltype(cod_layer)::ModuleHandleT;

  JIT(llvm::TargetMachine &target_machine, const llvm::DataLayout &data_layout);

  ModuleHandle addModule(std::unique_ptr<llvm::Module> module);

  llvm::JITSymbol findSymbol(const std::string name);

  llvm::JITTargetAddress getSymbolAddress(const std::string name);

  void removeModule(ModuleHandle handle);
};

} // end namespace JITSim

#endif // JITSIM_JIT_HPP_INCLUDED
