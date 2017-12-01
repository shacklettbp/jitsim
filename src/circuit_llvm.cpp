#include <jitsim/circuit_llvm.hpp>

namespace JITSim {

using namespace llvm;

static bool needsModule(const Definition &definition)
{
  const SimInfo &siminfo = definition.getSimInfo();
  if (!siminfo.isPrimitive()) {
    return true;
  }

  return siminfo.getPrimitive().has_definition;
}

static ModuleEnvironment ModuleForPrimitive(Builder &builder,
                                            const Definition &definition, 
                                            const Primitive &prim)
{
  assert(prim.has_definition);

  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName());

  prim.make_def(mod_env);

  return mod_env;
}

std::vector<Type *> getArgTypes(const Definition &definition, ModuleEnvironment &mod_env)
{
  std::vector<Type *> arg_types;
  for (const JITSim::Value &defn_input : definition.getIFace().getOutputs()) {
    arg_types.push_back(Type::getIntNTy(mod_env.getContext(), defn_input.getWidth()));
  }

  return arg_types;
}

static FunctionType * makeComputeOutputsType(const Definition &definition, ModuleEnvironment &mod_env) 
{
  std::string out_type_name = definition.getSafeName() + "_outputs";

  std::vector<Type *> arg_types = getArgTypes(definition, mod_env);
  std::vector<Type *> elem_types;
  for (const JITSim::Input &defn_output : definition.getIFace().getInputs()) {
    elem_types.push_back(Type::getIntNTy(mod_env.getContext(), defn_output.getWidth()));
  }

  StructType* ret_type = StructType::create(mod_env.getContext(), elem_types, out_type_name);

  return FunctionType::get(ret_type, arg_types, false);
}

static FunctionType * makeUpdateStateType(const Definition &definition, ModuleEnvironment &mod_env)
{
  std::vector<Type *> arg_types = getArgTypes(definition, mod_env);
  return FunctionType::get(Type::getVoidTy(mod_env.getContext()), arg_types, false);
}

ModuleEnvironment ModuleForDefinition(Builder &builder, const Definition &definition)
{
  const SimInfo &siminfo = definition.getSimInfo();
  if (siminfo.isPrimitive()) {
    return ModuleForPrimitive(builder, definition, siminfo.getPrimitive());
  }

  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName());
  FunctionType *co_type = makeComputeOutputsType(definition, mod_env);
  FunctionEnvironment compute_outputs = mod_env.makeFunction(definition.getSafeName() + "_compute_outputs", co_type);
  if (siminfo.isStateful()) {
    FunctionType *us_type = makeUpdateStateType(definition, mod_env);
    FunctionEnvironment update_state = mod_env.makeFunction(definition.getSafeName() + "_update_state", us_type);
  }

  return mod_env;
}

std::vector<ModuleEnvironment> ModulesForCircuit(Builder &builder, const Circuit &circuit)
{
  std::vector<ModuleEnvironment> modules;

  for (const Definition &defn : circuit.getDefinitions()) {
    if (needsModule(defn)) {
      auto mod = ModuleForDefinition(builder, defn);
      modules.emplace_back(std::move(mod));
    }
  }

  return modules;
}

}
