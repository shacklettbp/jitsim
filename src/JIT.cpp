#include <jitsim/JIT.hpp>
#include <iostream>

namespace JITSim {

using namespace llvm;
using namespace llvm::orc;

static __attribute__((constructor)) void initializeLLVM() 
{
  InitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();
}

JIT::JIT(TargetMachine &target_machine, const DataLayout &dl)
  : data_layout(dl),
    object_layer([]() { return std::make_shared<SectionMemoryManager>(); }),
    compile_layer(object_layer, SimpleCompiler(target_machine)),
    optimize_layer(compile_layer,
                  [this](std::shared_ptr<Module> module) {
                    return optimizeModule(std::move(module));
                  }),
    compile_callback_manager(
      createLocalCompileCallbackManager(target_machine.getTargetTriple(), 0)),
    cod_layer(optimize_layer,
              [](Function &fn) { return std::set<Function*>({&fn}); },
              *compile_callback_manager,
              createLocalIndirectStubsManagerBuilder(target_machine.getTargetTriple()))
{
  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}

JIT::ModuleHandle JIT::addModule(std::unique_ptr<Module> module) {
  assert(module->getDataLayout() == data_layout);

  // Build our symbol resolver:
  // Lambda 1: Look back into the JIT itself to find symbols that are part of
  //           the same "logical dylib".
  // Lambda 2: Search for external symbols in the host process.
  auto Resolver = createLambdaResolver(
    [&](const std::string &name) {
      if (auto sym = cod_layer.findSymbol(name, false))
        return sym;
      return JITSymbol(nullptr);
    },  
    [](const std::string &name) {
      if (auto sym_addr = RTDyldMemoryManager::getSymbolAddressInProcess(name))
        return JITSymbol(sym_addr, JITSymbolFlags::Exported);
      return JITSymbol(nullptr);
    }); 

  // Add the set to the JIT with the resolver we created above and a newly
  // created SectionMemoryManager.
  return cantFail(cod_layer.addModule(std::move(module), std::move(Resolver)));
}

JITSymbol JIT::findSymbol(const std::string name) {
  return cod_layer.findSymbol(mangle(name), true);
}   

JITTargetAddress JIT::getSymbolAddress(const std::string name) {
  return cantFail(findSymbol(name).getAddress());
} 

void JIT::removeModule(ModuleHandle handle) {
  cantFail(cod_layer.removeModule(handle));
}

std::string JIT::mangle(const std::string name) {
  std::string mangled_name;
  raw_string_ostream mangled_name_stream(mangled_name);
  Mangler::getNameWithPrefix(mangled_name_stream, name, data_layout);
  return mangled_name_stream.str();
}

std::shared_ptr<Module> JIT::optimizeModule(std::shared_ptr<Module> module) {
  // Create a function pass manager.
  auto fpm = make_unique<legacy::FunctionPassManager>(module.get());

  // Add some optimizations.
  fpm->add(createInstructionCombiningPass());
  fpm->add(createReassociatePass());
  fpm->add(createGVNPass());
  fpm->add(createCFGSimplificationPass());
  fpm->doInitialization();

  // Run the optimizations over all functions in the module being added to
  // the JIT.
  for (auto &fn : *module)
    fpm->run(fn);

  return module;
}

} // end namespace JITSim
