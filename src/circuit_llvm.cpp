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
  for (const Source &defn_sink : definition.getIFace().getSources()) {
    arg_types.push_back(Type::getIntNTy(mod_env.getContext(), defn_sink.getWidth()));
  }

  return arg_types;
}

std::string getComputeOutputName(const Definition &definition)
{
  return Twine(definition.getSafeName(), "_compute_outputs").str();
}

std::string getUpdateStateName(const Definition &definition)
{
  return Twine(definition.getSafeName(), "_update_state").str();
}

static StructType *makeReturnType(const Definition &definition, ModuleEnvironment &mod_env)
{
  std::string out_type_name = definition.getSafeName() + "_source_type";
  std::vector<Type *> elem_types;
  for (const JITSim::Sink &defn_source : definition.getIFace().getSinks()) {
    elem_types.push_back(Type::getIntNTy(mod_env.getContext(), defn_source.getWidth()));
  }

  return StructType::create(mod_env.getContext(), elem_types, out_type_name);
}

static FunctionType * makeComputeOutputType(const Definition &definition, ModuleEnvironment &mod_env) 
{
  Function *decl = mod_env.getFunctionDecl(getComputeOutputName(definition));
  if (decl) {
    return decl->getFunctionType();
  }

  bool is_stateful = definition.getSimInfo().isStateful();

  std::vector<Type *> arg_types;
  if (!is_stateful) { /* FIXME */
    arg_types = getArgTypes(definition, mod_env);
  }

  if (is_stateful) {
    arg_types.push_back(Type::getInt8PtrTy(mod_env.getContext()));
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
  arg_types.push_back(Type::getInt8PtrTy(mod_env.getContext()));

  return FunctionType::get(Type::getVoidTy(mod_env.getContext()), arg_types, false);
}

static Value * createSlice(Value *whole, int offset, int width, FunctionEnvironment &env)
{
  Value *cur = whole;
  if (offset > 0) {
    cur = env.getIRBuilder().CreateLShr(cur, offset);
  }

  return env.getIRBuilder().CreateTrunc(cur, Type::getIntNTy(env.getContext(), width));
}

/* Builds a larger integer out of a list of smaller slices of other integers */

static Value * makeValueReference(const Select &select, FunctionEnvironment &env)
{
  if (select.isDirect()) {
    const SourceSlice &slice = select.getDirect();
    if (slice.isConstant()) {
      const APInt &const_int = slice.getConstant();
      return ConstantInt::get(env.getContext(), const_int);
    }
    else {
      return env.lookupValue(slice.getSource());
    }
  } else {
    Value *acc = nullptr;
    int total_width = 0;
    for (const SourceSlice &slice : select.getSlices()) {
      Value *sliced_val;
      if (slice.isConstant()) {
        sliced_val = ConstantInt::get(env.getContext(), slice.getConstant());
      } else {
        Value *whole_val = env.lookupValue(slice.getSource());
        sliced_val = createSlice(whole_val, slice.getOffset(), slice.getWidth(), env);
      }
      total_width += slice.getWidth();

      if (!acc) {
        acc = sliced_val;
      } else {
        Value *src = env.getIRBuilder().CreateZExt(sliced_val, Type::getIntNTy(env.getContext(), total_width), "src");
        Value *dest = env.getIRBuilder().CreateZExt(acc, Type::getIntNTy(env.getContext(), total_width), "dst");

        src = env.getIRBuilder().CreateShl(src, total_width - slice.getWidth(), "src_shift");
        dest = env.getIRBuilder().CreateOr(dest, src, "concat");

        acc = dest;
      }
    }
    assert(acc);
    return acc ;
  }
}

static Value * incrementStatePtr(Value *cur_ptr, int incr, FunctionEnvironment &env)
{
  return env.getIRBuilder().CreateGEP(cur_ptr, ConstantInt::get(env.getContext(), APInt(64, incr, false)));
}

static void makeInstanceComputeOutput(const Instance *inst, const SimInfo &defn_info, FunctionEnvironment &env, Value *base_state)
{
  const SimInfo &info = inst->getDefinition().getSimInfo();
  const IFace &iface = inst->getIFace();
  const std::vector<Source> &sources = iface.getSources();

  std::vector<Value *> argument_values;

  if (!info.isStateful()) {
    /* FIXME do this port by port to support more circuits */
    for (const Sink &sink : iface.getSinks()) {
      argument_values.push_back(makeValueReference(sink.getSelect(), env));
    }
  }

  if (info.isStateful()) {
    Value *state_ptr = incrementStatePtr(base_state, defn_info.getOffset(inst), env);
    argument_values.push_back(state_ptr);
  }

  std::vector<Value *> ret_values;
  if (info.isPrimitive()) {
    const Primitive &prim = info.getPrimitive();
    ret_values = prim.make_compute_output(env, argument_values, *inst);
  } else {
    std::string inst_comp_sources = getComputeOutputName(inst->getDefinition());
    Function *inst_func = env.getModule().getFunctionDecl(inst_comp_sources);
    if (inst_func == nullptr) {
      inst_func = env.getModule().makeFunctionDecl(inst_comp_sources, makeComputeOutputType(inst->getDefinition(), env.getModule()));
    }

    Value *ret_struct = env.getIRBuilder().CreateCall(inst_func, argument_values, "inst_source");
    for (unsigned i = 0; i < sources.size(); i++) {
      Value *struct_elem = env.getIRBuilder().CreateExtractValue(ret_struct, { i });
      ret_values.push_back(struct_elem);
    }
  }

  for (unsigned i = 0; i < ret_values.size(); i++) {
    env.addValue(&sources[i], ret_values[i]);
  }
}

static void makeInstanceUpdateState(const Instance *inst, const SimInfo &defn_info, FunctionEnvironment &env, Value *base_state)
{
  const SimInfo &info = inst->getDefinition().getSimInfo();
  const IFace &iface = inst->getIFace();

  std::vector<Value *> argument_values;

  for (const Sink &sink : iface.getSinks()) {
    argument_values.push_back(makeValueReference(sink.getSelect(), env));
  }

  Value *state_ptr = incrementStatePtr(base_state, defn_info.getOffset(inst), env);
  argument_values.push_back(state_ptr);

  std::vector<Value *> ret_values;
  if (info.isPrimitive()) {
    const Primitive &prim = info.getPrimitive();
    prim.make_update_state(env, argument_values, *inst);
  } else {
    std::string inst_update_state = getUpdateStateName(inst->getDefinition());
    Function *inst_func = env.getModule().getFunctionDecl(inst_update_state);
    if (inst_func == nullptr) {
      inst_func = env.getModule().makeFunctionDecl(inst_update_state, makeUpdateStateType(inst->getDefinition(), env.getModule()));
    }

    env.getIRBuilder().CreateCall(inst_func, argument_values);
  }
}

static void makeComputeOutputDefn(const Definition &definition, ModuleEnvironment &mod_env)
{
  const SimInfo &defn_info = definition.getSimInfo();
  bool is_stateful = defn_info.isStateful();

  FunctionType *co_type = makeComputeOutputType(definition, mod_env);
  FunctionEnvironment compute_outputs = mod_env.makeFunction(getComputeOutputName(definition), co_type);
  compute_outputs.addBasicBlock("entry");

  const std::vector<Source> & sources = definition.getIFace().getSources();
  auto arg = compute_outputs.getFunction()->arg_begin();
  if (!is_stateful) {
    for (unsigned i = 0; i < sources.size(); i++, arg++) {
      const Source *src = &sources[i];

      compute_outputs.addValue(src, arg);
      arg->setName("self." + src->getName());
    }
  }

  Value *state_ptr = nullptr;
  if (is_stateful) {
    state_ptr = compute_outputs.getFunction()->arg_end() - 1;
    state_ptr->setName("state_ptr");
  }

  const std::vector<const Instance *> &output_deps = defn_info.getOutputDeps();
  for (const Instance *inst : output_deps) {
    makeInstanceComputeOutput(inst, defn_info, compute_outputs, state_ptr);
  }

  const std::vector<JITSim::Sink> & sinks = definition.getIFace().getSinks();
  Value *ret_val = UndefValue::get(co_type->getReturnType());

  for (unsigned i = 0; i < sinks.size(); i++ ) {
    Value *ret_part = makeValueReference(sinks[i].getSelect(), compute_outputs);
    ret_val = compute_outputs.getIRBuilder().CreateInsertValue(ret_val, ret_part, { i });
  }

  compute_outputs.getIRBuilder().CreateRet(ret_val);

  compute_outputs.verify();
}

static void makeUpdateStateDefn(const Definition &definition, ModuleEnvironment &mod_env)
{
  const SimInfo &defn_info = definition.getSimInfo();

  FunctionType *us_type = makeUpdateStateType(definition, mod_env);
  FunctionEnvironment update_state = mod_env.makeFunction(getUpdateStateName(definition), us_type);
  update_state.addBasicBlock("entry");

  const std::vector<Source> & sources = definition.getIFace().getSources();
  auto arg = update_state.getFunction()->arg_begin();

  for (unsigned i = 0; i < sources.size(); i++, arg++) {
    const Source *src = &sources[i];

    update_state.addValue(src, arg);
    arg->setName("self." + src->getName());
  }

  Value *state_ptr = update_state.getFunction()->arg_end() - 1;
  state_ptr->setName("state_ptr");

  for (const Instance *inst : defn_info.getStateDeps()) {
    makeInstanceComputeOutput(inst, defn_info, update_state, state_ptr);
  }

  for (const Instance *inst : defn_info.getStatefulInstances()) {
    makeInstanceUpdateState(inst, defn_info, update_state, state_ptr);
  }

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

  makeComputeOutputDefn(definition, mod_env);

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
