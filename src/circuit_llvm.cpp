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
  std::string out_type_name = definition.getSafeName() + "_output_type";
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

  std::vector<Type *> arg_types;
  if (!definition.getSimInfo().isStateful()) {
      arg_types = getArgTypes(definition, mod_env);
  }
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

static llvm::Value * createSlice(llvm::Value *whole, int offset, int width, FunctionEnvironment &env)
{
  llvm::Value *cur = whole;
  if (offset > 0) {
    cur = env.getIRBuilder().CreateLShr(cur, offset);
  }

  return env.getIRBuilder().CreateTrunc(cur, Type::getIntNTy(env.getContext(), width));
}

static llvm::Value * makeValueReference(const Select &select, FunctionEnvironment &env)
{
  if (select.isDirect()) {
    const ValueSlice &slice = select.getDirect();
    if (slice.isConstant()) {
      const APInt &const_int = slice.getConstant();
      return ConstantInt::get(env.getContext(), const_int);
    }
    else {

      return env.lookupValue(slice.getValue());
    }
  } else {
    llvm::Value *acc = nullptr;
    int total_width = 0;
    for (const ValueSlice &slice : select.getSlices()) {
      llvm::Value *sliced_val;
      if (slice.isConstant()) {
        sliced_val = ConstantInt::get(env.getContext(), slice.getConstant());
      } else {
        llvm::Value *whole_val = env.lookupValue(slice.getValue());
        sliced_val = createSlice(whole_val, slice.getOffset(), slice.getWidth(), env);
      }
      total_width += slice.getWidth();

      if (!acc) {
        acc = sliced_val;
      } else {
        llvm::Value *src = env.getIRBuilder().CreateZExt(sliced_val, Type::getIntNTy(env.getContext(), total_width));
        llvm::Value *dest = env.getIRBuilder().CreateZExt(acc, Type::getIntNTy(env.getContext(), total_width));

        /* FIXME is this actually correct */
        src = env.getIRBuilder().CreateShl(src, total_width - slice.getWidth());
        dest = env.getIRBuilder().CreateOr(dest, src);

        acc = dest;
      }
    }
    assert(acc);
    return acc ;
  }
}

static void makeInstanceComputeOutputs(const Instance *inst, FunctionEnvironment &env)
{
  const SimInfo &info = inst->getDefinition().getSimInfo();
  const IFace &iface = inst->getIFace();
  const std::vector<JITSim::Value> &outputs = iface.getOutputs();

  std::vector<llvm::Value *> argument_values;

  if (!info.isStateful()) {
    /* FIXME do this input by input to support more circuits */
    for (const Input &input : iface.getInputs()) {
      argument_values.push_back(makeValueReference(input.getSelect(), env));
    }
  }

  std::vector<llvm::Value *> ret_values;
  if (info.isPrimitive()) {
    const Primitive &prim = info.getPrimitive();
    ret_values = prim.make_compute_output(env, argument_values);
  } else {
    std::string inst_comp_outputs = getComputeOutputsName(inst->getDefinition());
    Function *inst_func = env.getModule().getFunctionDecl(inst_comp_outputs);
    if (inst_func == nullptr) {
      inst_func = env.getModule().makeFunctionDecl(inst_comp_outputs, makeComputeOutputsType(inst->getDefinition(), env.getModule()));
    }

    llvm::Value *ret_struct = env.getIRBuilder().CreateCall(inst_func, argument_values, "inst_output");
    for (unsigned i = 0; i < outputs.size(); i++) {
      llvm::Value *struct_elem = env.getIRBuilder().CreateExtractValue(ret_struct, { i });
      ret_values.push_back(struct_elem);
    }
  }

  for (unsigned i = 0; i < ret_values.size(); i++) {
    env.addValue(&outputs[i], ret_values[i]);
  }
}

static void makeComputeOutputsDefn(const Definition &definition, ModuleEnvironment &mod_env)
{
  const SimInfo &defn_info = definition.getSimInfo();
  FunctionType *co_type = makeComputeOutputsType(definition, mod_env);
  FunctionEnvironment compute_outputs = mod_env.makeFunction(getComputeOutputsName(definition), co_type);
  compute_outputs.addBasicBlock("entry");

  int idx = 0;
  const std::vector<JITSim::Value> & outputs = definition.getIFace().getOutputs();
  for (llvm::Value &arg : compute_outputs.getFunction()->args()) {
    const JITSim::Value *val = &outputs[idx];

    compute_outputs.addValue(val, &arg);
    arg.setName("self." + val->getName());

    idx++;
  }

  for (const Instance *inst : defn_info.getOutputDeps()) {
    makeInstanceComputeOutputs(inst, compute_outputs);
  }

  const std::vector<JITSim::Input> & inputs = definition.getIFace().getInputs();
  llvm::Value *ret_val = UndefValue::get(co_type->getReturnType());

  for (unsigned i = 0; i < inputs.size(); i++ ) {
    llvm::Value *ret_part = makeValueReference(inputs[i].getSelect(), compute_outputs);
    ret_val = compute_outputs.getIRBuilder().CreateInsertValue(ret_val, ret_part, { i });
  }

  compute_outputs.getIRBuilder().CreateRet(ret_val);

  compute_outputs.verify();
}

static void makeUpdateStateDefn(const Definition &definition, ModuleEnvironment &mod_env)
{
    FunctionType *us_type = makeUpdateStateType(definition, mod_env);
    FunctionEnvironment update_state = mod_env.makeFunction(getUpdateStateName(definition), us_type);
    update_state.addBasicBlock("entry");

    update_state.getIRBuilder().CreateRetVoid();

    update_state.verify();
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
