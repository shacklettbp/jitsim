#include <jitsim/JIT.hpp>
#include <iostream>

#include <llvm/Support/raw_os_ostream.h>

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
    compile_callback_manager(
      createLocalCompileCallbackManager(target_machine.getTargetTriple(), 0)),
    indirect_stubs_manager(
      createLocalIndirectStubsManagerBuilder(target_machine.getTargetTriple())()),
    object_layer([]() { return std::make_shared<SectionMemoryManager>(); }),
    compile_layer(object_layer, SimpleCompiler(target_machine)),
    optimize_layer(compile_layer,
                  [this](std::shared_ptr<Module> module) {
                    return optimizeModule(std::move(module));
                  }),
    debug_layer(optimize_layer,
                [this](std::shared_ptr<Module> module) {
                  return debugModule(std::move(module));
                }),
    debug_print_ir(false)
{
  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}

JIT::~JIT()
{
  for (ModuleHandle &handle : modules) {
    removeModule(handle);
  }
}

JIT::ModuleHandle JIT::addModule(std::unique_ptr<Module> module, bool is_debug) {
  assert(module->getDataLayout() == data_layout);

  // Build our symbol resolver:
  // Lambda 1: Look back into the JIT itself to find symbols that are part of
  //           the same "logical dylib".
  // Lambda 2: Search for external symbols in the host process.
  auto resolver = createLambdaResolver(
    [&](const std::string &name) {
      if (auto sym = indirect_stubs_manager->findStub(name, false)) {
        return sym;
      } else if (auto sym = debug_layer.findSymbol(name, false)) {
        return sym;
      }
      return JITSymbol(nullptr);
    },  
    [](const std::string &name) {
      if (auto sym_addr = RTDyldMemoryManager::getSymbolAddressInProcess(name))
        return JITSymbol(sym_addr, JITSymbolFlags::Exported);
      return JITSymbol(nullptr);
    }); 

  // Add the set to the JIT with the resolver we created above and a newly
  // return created SectionMemoryManager.
  ModuleHandle handle = cantFail(debug_layer.addModule(std::move(module), std::move(resolver)));
  modules.push_back(handle); // FIXME need to put this into a more intelligent data type 

  return handle;
}

JITSymbol JIT::findSymbol(const std::string name) {
  if (auto sym = indirect_stubs_manager->findStub(mangle(name), true)) {
    return sym;
  } else if (auto sym = debug_layer.findSymbol(mangle(name), true)) {
    return sym;
  } else {
    return JITSymbol(nullptr);
  }
}   

JITTargetAddress JIT::getSymbolAddress(const std::string name) {
  return cantFail(findSymbol(name).getAddress());
} 

void JIT::removeModule(ModuleHandle handle) {
  cantFail(debug_layer.removeModule(handle));
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

std::shared_ptr<Module> JIT::debugModule(std::shared_ptr<Module> module)
{
  if (debug_print_ir) {
    outs() << "\n==============\n";
    outs() << *module;
    outs() << "\n==============\n";
  }
  return module;
}

std::string JIT::mangle(const std::string name) {
  std::string mangled_name;
  raw_string_ostream mangled_name_stream(mangled_name);
  Mangler::getNameWithPrefix(mangled_name_stream, name, data_layout);
  return mangled_name_stream.str();
}

void JIT::addLazyFunction(std::string name,
                          std::function<std::unique_ptr<Module>()> module_generator,
                          JIT::TransformFunction debug_transform)
{
  auto compile_callback = compile_callback_manager->getCompileCallback();

  cantFail(indirect_stubs_manager->createStub(mangle(name),
                                              compile_callback.getAddress(),
                                              JITSymbolFlags::Exported));

  compile_callback.setCompileAction([this, name, module_generator]() {
    auto M = module_generator();
    addModule(std::move(M));


    auto symbol = debug_layer.findSymbol(name, false);
    assert(symbol && "Couldn't find compiled function?");

    JITTargetAddress addr = cantFail(symbol.getAddress());
    if (auto err = indirect_stubs_manager->updatePointer(mangle(name), addr)) {
      logAllUnhandledErrors(std::move(err), errs(),
                            "Error updating function pointer: ");
    }

    return addr;
  });
  callback_addrs.insert(compile_callback.getAddress());
}

void JIT::precompileIR()
{
  for (const JITTargetAddress &addr : callback_addrs) {
    compile_callback_manager->executeCompileCallback(addr);
  }
}

void JIT::precompileDumpIR()
{
  debug_print_ir = true;
  precompileIR();
  debug_print_ir = false;
}

} // end namespace JITSim
