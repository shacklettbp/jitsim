#include <jitsim/JIT.hpp>
#include <iostream>
#include <tuple>

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

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

JIT::ModuleHandle JIT::addModule(std::shared_ptr<Module> module) {
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

  /* FIXME revisit this */
  PassManagerBuilder manager_builder;
  manager_builder.OptLevel = 2;
  manager_builder.populateFunctionPassManager(*fpm);

  // Add some optimizations.
  //fpm->add(createInstructionCombiningPass(true));
  //fpm->add(createReassociatePass());
  //fpm->add(createGVNPass());
  //fpm->add(createCFGSimplificationPass());
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

  auto iter = debug_functions.find(module->getName());
  if (iter == debug_functions.end()) {
    return module;
  }

  return iter->second(move(CloneModule(module.get())));
}

std::string JIT::mangle(const std::string name) {
  std::string mangled_name;
  raw_string_ostream mangled_name_stream(mangled_name);
  Mangler::getNameWithPrefix(mangled_name_stream, name, data_layout);
  return mangled_name_stream.str();
}

JITTargetAddress JIT::updateStub(const std::string &name)
{
  auto symbol = debug_layer.findSymbol(name, false);
  assert(symbol && "Couldn't find compiled function?");

  JITTargetAddress addr = cantFail(symbol.getAddress());
  if (auto err = indirect_stubs_manager->updatePointer(mangle(name), addr)) {
    logAllUnhandledErrors(std::move(err), errs(),
                          "Error updating function pointer: ");
  }

  return addr;
}

void JIT::addLazyFunction(std::string name,
                          std::function<std::unique_ptr<Module>()> module_generator,
                          JIT::TransformFunction debug_transform)
{
  auto compile_callback = compile_callback_manager->getCompileCallback();
  JITTargetAddress callback_address = compile_callback.getAddress();

  cantFail(indirect_stubs_manager->createStub(mangle(name),
                                              callback_address,
                                              JITSymbolFlags::Exported));

  bool is_debug = debug_transform != nullptr;
  if (is_debug) {
    debug_functions[name] = move(debug_transform);
  }

  compile_callback.setCompileAction([this, name, module_generator, is_debug, callback_address]() {
    auto module = module_generator();
    std::shared_ptr<Module> shared_module(std::move(module));
    auto compiled_handle = addModule(shared_module);
    if (is_debug) {
      debug_modules.emplace_back(name, compiled_handle, shared_module);
    }

    callback_addrs.erase(callback_address);

    return updateStub(name);
  });
  callback_addrs.insert(callback_address);
}

void JIT::precompileIR()
{
  std::unordered_set<JITTargetAddress> addrs_cpy(callback_addrs);
  for (const JITTargetAddress &addr : addrs_cpy) {
    compile_callback_manager->executeCompileCallback(addr);
  }
}

void JIT::precompileDumpIR()
{
  debug_print_ir = true;
  precompileIR();
  debug_print_ir = false;
}

void JIT::purgeDebugModules()
{
  auto debug_copy(move(debug_modules));
  debug_modules.clear();

  for (auto &elem : debug_copy) {
    auto name = std::get<0>(elem);
    auto compiled_handle = std::get<1>(elem);
    auto ir_module = std::get<2>(elem);
    removeModule(compiled_handle);

    auto compile_callback = compile_callback_manager->getCompileCallback();
    compile_callback.setCompileAction([this, name, ir_module] {
      auto new_handle = addModule(ir_module);
      debug_modules.emplace_back(name, new_handle, ir_module);

      return updateStub(name);
    });

    if (auto err = indirect_stubs_manager->updatePointer(mangle(name), compile_callback.getAddress())) {
      logAllUnhandledErrors(std::move(err), errs(),
                            "Error updating function pointer: ");
    }
  }
}

} // end namespace JITSim
