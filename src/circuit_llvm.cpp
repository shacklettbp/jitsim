#include <jitsim/circuit_llvm.hpp>

namespace JITSim {

using namespace llvm;

static std::unique_ptr<Module> ModuleForPrimitive(const Primitive &prim)
{
  if (!prim.has_definition) {
    return nullptr;
  }
  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName());

  prim.make_def(mod_env);

  return mod_env.returnModule();
}

static FunctionType * makeFuncType(const Definition &definition) 
{
  string in_name = definition.getSafeName() + "_inputs";
  string out_name = definition.getSafeName() + "_outputs";


}

std::unique_ptr<Module> ModuleForDefinition(Builder &builder, const Definition &definition)
{
  const SimInfo &siminfo = definition.getSimInfo();
  if (siminfo.isPrimitive()) {
    return ModuleForPrimitive(siminfo.getPrimitive());
  }

  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName());
  FunctionType *func_type = makeFuncType(definition, mod_env.getContext());
  FunctionEnvironment func_env = mod_env.makeFunction(definition.getSafeName(), func_type);

  return mod_env.returnModule();
}

std::vector<std::unique_ptr<Module>> ModulesForCircuit(Builder &builder, const Circuit &circuit)
{
  std::vector<std::unique_ptr<Module>> modules;

  for (const Definition &defn : circuit.getDefinitions()) {
    std::unique_ptr<Module> mod = ModuleForDefinition(builder, defn);
    if (mod != nullptr) {
      modules.push_back(mod);
    }
  }

  return modules;
}

}
