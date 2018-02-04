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
#include <llvm/ExecutionEngine/Orc/IndirectionUtils.h>
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
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <iostream>
#include <vector>

namespace JITSim {

class JIT {
private:
  const llvm::DataLayout data_layout;
  std::unique_ptr<llvm::orc::JITCompileCallbackManager> compile_callback_manager;
  std::unique_ptr<llvm::orc::IndirectStubsManager> indirect_stubs_manager;
  using TransformFunction =
      std::function<std::shared_ptr<llvm::Module>(std::shared_ptr<llvm::Module>)>;

  /* All the layers used by the JIT. These operate bottom up
   * (so the debug layer is run on top of all the other layers) */
  llvm::orc::RTDyldObjectLinkingLayer object_layer;
  llvm::orc::IRCompileLayer<decltype(object_layer), llvm::orc::SimpleCompiler> compile_layer;
  llvm::orc::IRTransformLayer<decltype(compile_layer), TransformFunction> optimize_layer;
  llvm::orc::IRTransformLayer<decltype(optimize_layer), TransformFunction> debug_layer;

  std::shared_ptr<llvm::Module> optimizeModule(std::shared_ptr<llvm::Module> module);
  std::shared_ptr<llvm::Module> debugModule(std::shared_ptr<llvm::Module> module);
  std::string mangle(const std::string name);

  using ModuleHandle = decltype(debug_layer)::ModuleHandleT;
  std::vector<std::tuple<std::string, ModuleHandle, std::shared_ptr<llvm::Module>>> debug_modules;
  std::unordered_map<std::string, TransformFunction> debug_functions;
  std::unordered_set<llvm::JITTargetAddress> callback_addrs;

  void removeModule(ModuleHandle handle);
  llvm::JITTargetAddress updateStub(const std::string &name);

  bool debug_print_ir;

public:

  JIT(llvm::TargetMachine &target_machine, const llvm::DataLayout &data_layout);

  llvm::JITSymbol findSymbol(const std::string name);

  llvm::JITTargetAddress getSymbolAddress(const std::string name);

  ModuleHandle addModule(std::shared_ptr<llvm::Module> module);
  void addLazyFunction(std::string name, std::function<std::shared_ptr<llvm::Module>()> module_generator,
                       TransformFunction debug_transform = nullptr);

  void precompileIR();
  void precompileDumpIR();

  void purgeDebugModules();
};

} // end namespace JITSim

#endif // JITSIM_JIT_HPP_INCLUDED
