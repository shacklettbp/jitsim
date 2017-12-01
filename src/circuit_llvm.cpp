#include <jitsim/circuit_llvm.hpp>

namespace JITSim {

using namespace llvm;

std::unique_ptr<Module> ModuleForDefinition(Builder &builder, const Definition &definition)
{
  ModuleEnvironment mod_env = builder.makeModule(definition.getName());
  const SimInfo &siminfo = definition.getSimInfo();

  (void)siminfo;

  return mod_env.returnModule();
}

std::vector<std::unique_ptr<Module>> ModulesForCircuit(Builder &builder, const Circuit &circuit)
{
  std::vector<std::unique_ptr<Module>> modules;

  for (const Definition &defn : circuit.getDefinitions()) {
    modules.push_back(ModuleForDefinition(builder, defn));
  }

  return modules;
}

}
