#include <jitsim/circuit_llvm.hpp>

namespace JITSim {

std::unique_ptr<llvm::Module> ModuleForDefinition(Builder &builder, const Definition &definition)
{
  ModuleEnvironment mod_env = builder.MakeModule(definition.getName());
  const SimInfo &siminfo = definition.getSimInfo();

  (void)siminfo;

  return mod_env.returnModule();
}

};
