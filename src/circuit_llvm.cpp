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

std::string getComputeOutputsName(const Definition &definition)
{
  return Twine(definition.getSafeName(), "_compute_outputs").str();
}

std::string getUpdateStateName(const Definition &definition)
{
  return Twine(definition.getSafeName(), "_update_state").str();
}

static StructType *makeReturnType(const Definition &definition, ModuleEnvironment &mod_env)
{
  std::vector<Type *> elem_types;
  for (const JITSim::Input &defn_output : definition.getIFace().getInputs()) {
    elem_types.push_back(Type::getIntNTy(mod_env.getContext(), defn_output.getWidth()));
  }

  return StructType::create(mod_env.getContext(), elem_types, out_type_name);
}

static FunctionType * makeComputeOutputsType(const Definition &definition, ModuleEnvironment &mod_env) 
{
  Function *decl = mod_env.getFunctionDecl(getComputeOutputsName(definition));
  if (decl) {
    return decl->getFunctionType();
  }

  std::string out_type_name = definition.getSafeName() + "_output_type";

  std::vector<Type *> arg_types = getArgTypes(definition, mod_env);
  StructType *ret_type = makeReturnType(definition, mod_env);

  return FunctionType::get(ret_type, arg_types, false);
}

static FunctionType * makeUpdateStateType(const Definition &definition, ModuleEnvironment &mod_env)
{
  Function *decl = mod_env.getFunctionDecl(getUpdateStateName(definition));
  if (decl) {
    return decl->getFunctionType();
  }

  std::vector<Type *> arg_types = getArgTypes(definition, mod_env);
  return FunctionType::get(Type::getVoidTy(mod_env.getContext()), arg_types, false);
}

static llvm::Value * makeValueReference(const Select &select, FunctionEnvironment &env)
{
  if (select.isDirect()) {
    JITSim::Value *source = select.getDirect();
    return env.lookupValue(source);
  } else {
    for (const ValueSlice &slice : select.getSlices()) {
    }
  }
}

static void makeInstanceComputeOutputs(const Instance *inst, FunctionEnvironment &env)
{
  const SimInfo &info = inst->getDefinition().getSimInfo();
  const vector<JITSim::Value *> outputs = iface.getOutputs();

  std::vector<Value *> argument_values;
  const IFace &iface = inst->getIFace();

  for (const Input &input : iface.getInputs()) {
    argument_values.push_back(makeValueReference(input.getSelect()));
  }

  vector<llvm::Value *> ret_values;
  if (info.isPrimitive()) {
    const Primitive &prim = info.getPrimitive();
    ret_values = prim.make_compute_outputs(env, argument_values, rettype);
  } else {
    std::string inst_comp_outputs = getComputeOutputsName(inst->getDefinition())
    Function *inst_func = env.getModule().getFunctionDecl(inst_comp_outputs);
    if (inst_func == nullptr) {
      inst_func = env.getModule().makeFunctionDecl(inst_comp_outputs, makeComputeOutputsType(inst->getDefinition(), env.getModule()));
    }

    llvm::Value *ret_struct = env.getIRBuilder().CreateCall(inst_func, argument_values, "inst_output");
    for (unsigned i = 0; i < outputs.size(); i++) {
      llvm::Value *struct_elem = env.getIRBuilder().CreateExtractValue(ret_struct, { i });
      ret_Values.push_back(struct_elem);
    }
  }

  for (unsigned i = 0; i < ret_values.size(); i++) {
    env.addValue(outputs[i], retValues[i]);
  }
}

static void makeComputeOutputsDefn(const Definition &definition, ModuleEnvironment &mod_env)
{
  const SimInfo &defn_info = definition.getSimInfo();
  FunctionType *co_type = makeComputeOutputsType(definition, mod_env);
  FunctionEnvironment compute_outputs = mod_env.makeFunction(getComputeOutputsName(definition), co_type);
  compute_outputs.addBasicBlock("entry");

  int idx = 0;
  for (llvm::Value &arg : compute_outputs.getFunction()->args()) {
    const JITSim::Value *val = &definition.getIFace().getOutputs()[idx];

    compute_outputs.addValue(val, &arg);
    arg.setName("self." + val->getName());

    idx++;
  }

  for (const Instance *inst : defn_info.getOutputDeps()) {
    makeInstanceComputeOutputs(inst, compute_outputs);
  }
}

static void makeUpdateStateDefn(const Definition &definition, ModuleEnvironment &mod_env)
{
    FunctionType *us_type = makeUpdateStateType(definition, mod_env);
    FunctionEnvironment update_state = mod_env.makeFunction(getUpdateStateName(definition), us_type);
}

ModuleEnvironment ModuleForDefinition(Builder &builder, const Definition &definition)
{
  const SimInfo &siminfo = definition.getSimInfo();
  if (siminfo.isPrimitive()) {
    return ModuleForPrimitive(builder, definition, siminfo.getPrimitive());
  }

  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName());

  makeComputeOutputsDefn(definition, mod_env);

  if (siminfo.isStateful()) {
    makeUpdateStateDefn(definition, mod_env);
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
