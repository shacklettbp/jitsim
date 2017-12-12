#include <jitsim/circuit_llvm.hpp>
#include "llvm_utils.hpp"

namespace JITSim {

using namespace llvm;

std::vector<Type *> getArgTypes(const std::vector<const Source *> &sources, ModuleEnvironment &mod_env)
{
  std::vector<Type *> arg_types;
  for (const Source *src: sources) {
    arg_types.push_back(Type::getIntNTy(mod_env.getContext(), src->getWidth()));
  }

  return arg_types;
}

std::string getComputeOutputName(const Definition &definition)
{
  return Twine(definition.getSafeName(), "_compute_output").str();
}

std::string getUpdateStateName(const Definition &definition)
{
  return Twine(definition.getSafeName(), "_update_state").str();
}

static StructType *makeReturnType(const Definition &definition, ModuleEnvironment &mod_env)
{
  std::string out_type_name = definition.getSafeName() + "_output_type";
  return ConstructStructType(definition.getIFace().getSinks(), mod_env.getContext(), out_type_name);
}

static FunctionType * makeComputeOutputType(const Definition &definition, ModuleEnvironment &mod_env) 
{
  const SimInfo &sim_info = definition.getSimInfo();
  Function *decl = mod_env.getFunctionDecl(getComputeOutputName(definition));
  if (decl) {
    return decl->getFunctionType();
  }

  std::vector<Type *> arg_types;
  arg_types = getArgTypes(sim_info.getOutputSources(), mod_env);

  if (sim_info.isStateful()) {
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

  std::vector<Type *> arg_types = getArgTypes(definition.getSimInfo().getStateSources(), mod_env);
  arg_types.push_back(Type::getInt8PtrTy(mod_env.getContext()));

  return FunctionType::get(Type::getVoidTy(mod_env.getContext()), arg_types, false);
}

static FunctionType * makeOutputDepsType(const Definition &definition, ModuleEnvironment &mod_env)
{
  const SimInfo &sim_info = definition.getSimInfo();
  Function *decl = mod_env.getFunctionDecl(definition.getSafeName() + "_output_deps");
  if (decl) {
    return decl->getFunctionType();
  }

  std::vector<Type *> arg_types = getArgTypes(definition.getSimInfo().getOutputSources(), mod_env);
  if (sim_info.isStateful()) {
    arg_types.push_back(Type::getInt8PtrTy(mod_env.getContext()));
  }

  arg_types.push_back(Type::getInt64Ty(mod_env.getContext()));
  arg_types.push_back(Type::getInt8PtrTy(mod_env.getContext()));

  return FunctionType::get(makeReturnType(definition, mod_env), arg_types, false);
}

static FunctionType * makeStateDepsType(const Definition &definition, ModuleEnvironment &mod_env)
{
  Function *decl = mod_env.getFunctionDecl(definition.getSafeName() + "_state_deps");
  if (decl) {
    return decl->getFunctionType();
  }

  std::vector<Type *> arg_types = getArgTypes(definition.getSimInfo().getStateSources(), mod_env);
  arg_types.push_back(Type::getInt8PtrTy(mod_env.getContext()));
  arg_types.push_back(Type::getInt64Ty(mod_env.getContext()));
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
  if (incr == 0) {
    return cur_ptr;
  } else {
    return env.getIRBuilder().CreateConstInBoundsGEP1_64(cur_ptr, incr);
  }
}

static void makeInstanceComputeOutput(const Instance *inst, const SimInfo &defn_info, FunctionEnvironment &env, Value *base_state)
{
  const SimInfo &inst_info = inst->getDefinition().getSimInfo();
  const InstanceIFace &iface = inst->getIFace();
  const std::vector<Source> &sources = iface.getSources();

  std::vector<Value *> argument_values;

  for (const Source *src : inst_info.getOutputSources()) {
    const Sink *sink = iface.getSink(src);
    argument_values.push_back(makeValueReference(sink->getSelect(), env));
  }

  if (inst_info.isStateful()) {
    Value *state_ptr = incrementStatePtr(base_state, defn_info.getOffset(inst), env);
    argument_values.push_back(state_ptr);
  }

  std::vector<Value *> ret_values;
  if (inst_info.isPrimitive()) {
    const Primitive &prim = inst_info.getPrimitive();
    ret_values = prim.make_compute_output(env, argument_values, *inst);
  } else {
    std::string inst_comp_output = getComputeOutputName(inst->getDefinition());
    Function *inst_func = env.getModule().getFunctionDecl(inst_comp_output);
    if (inst_func == nullptr) {
      inst_func = env.getModule().makeFunctionDecl(inst_comp_output, makeComputeOutputType(inst->getDefinition(), env.getModule()));
    }

    Value *ret_struct = env.getIRBuilder().CreateCall(inst_func, argument_values, inst->getName() + "_output");
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
  const SimInfo &inst_info = inst->getDefinition().getSimInfo();
  const InstanceIFace &iface = inst->getIFace();

  std::vector<Value *> argument_values;

  for (const Source *src : inst_info.getStateSources()) {
    const Sink *sink = iface.getSink(src);
    argument_values.push_back(makeValueReference(sink->getSelect(), env));
  }

  Value *state_ptr = incrementStatePtr(base_state, defn_info.getOffset(inst), env);
  argument_values.push_back(state_ptr);

  if (inst_info.isPrimitive()) {
    const Primitive &prim = inst_info.getPrimitive();
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

std::unique_ptr<Module> MakeComputeOutput(Builder &builder, const Definition &definition)
{
  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName() + "_compute_output");

  const SimInfo &defn_info = definition.getSimInfo();

  FunctionType *co_type = makeComputeOutputType(definition, mod_env);
  FunctionEnvironment compute_output = mod_env.makeFunction(getComputeOutputName(definition), co_type);
  compute_output.addBasicBlock("entry");

  const std::vector<const Source *> &sources = defn_info.getOutputSources();
  auto arg = compute_output.getFunction()->arg_begin();
  assert(compute_output.getFunction()->arg_size() == sources.size() + defn_info.isStateful());

  for (unsigned i = 0; i < sources.size(); i++, arg++) {
    const Source *src = sources[i];

    compute_output.addValue(src, arg);
    arg->setName("self." + src->getName());
  }
  
  Value *state_ptr = nullptr;
  if (defn_info.isStateful()) {
    state_ptr = compute_output.getFunction()->arg_end() - 1;
    state_ptr->setName("state_ptr");
  }

  const std::vector<const Instance *> &output_deps = defn_info.getOutputDeps();
  for (const Instance *inst : output_deps) {
    makeInstanceComputeOutput(inst, defn_info, compute_output, state_ptr);
  }

  const std::vector<JITSim::Sink> & sinks = definition.getIFace().getSinks();
  Value *ret_val = UndefValue::get(co_type->getReturnType());

  for (unsigned i = 0; i < sinks.size(); i++ ) {
    Value *ret_part = makeValueReference(sinks[i].getSelect(), compute_output);
    ret_val = compute_output.getIRBuilder().CreateInsertValue(ret_val, ret_part, { i });
  }

  compute_output.getIRBuilder().CreateRet(ret_val);

  assert(!compute_output.verify());
  
  return mod_env.returnModule();
}

std::unique_ptr<Module> MakeUpdateState(Builder &builder, const Definition &definition)
{
  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName() + "_update_state");

  const SimInfo &defn_info = definition.getSimInfo();

  FunctionType *us_type = makeUpdateStateType(definition, mod_env);
  FunctionEnvironment update_state = mod_env.makeFunction(getUpdateStateName(definition), us_type);
  update_state.addBasicBlock("entry");

  const std::vector<const Source *> & sources = defn_info.getStateSources();
  auto arg = update_state.getFunction()->arg_begin();
  assert(update_state.getFunction()->arg_size() == sources.size() + 1);

  for (unsigned i = 0; i < sources.size(); i++, arg++) {
    const Source *src = sources[i];

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
  assert(!update_state.verify());
  assert(!mod_env.verify());

  return mod_env.returnModule();
}

static void makeInstanceOutputDeps(const Instance *inst, const SimInfo &defn_info, FunctionEnvironment &env, Value *base_state, Value *inst_offset, Value *value_store)
{
  const SimInfo &inst_info = inst->getDefinition().getSimInfo();
  const InstanceIFace &iface = inst->getIFace();
  const std::vector<Source> &sources = iface.getSources();

  std::vector<Value *> argument_values;

  for (const Source *src : inst_info.getOutputSources()) {
    const Sink *sink = iface.getSink(src);
    argument_values.push_back(makeValueReference(sink->getSelect(), env));
  }

  if (inst_info.isStateful()) {
    Value *state_ptr = incrementStatePtr(base_state, defn_info.getOffset(inst), env);
    argument_values.push_back(state_ptr);
  }

  std::vector<Value *> ret_values;
  if (inst_info.isPrimitive()) {
    const Primitive &prim = inst_info.getPrimitive();
    ret_values = prim.make_compute_output(env, argument_values, *inst);
  } else {
    inst_offset = env.getIRBuilder().CreateAdd(inst_offset, ConstantInt::get(env.getContext(), APInt(64, defn_info.getInstNum(inst))));
    argument_values.push_back(inst_offset);
    argument_values.push_back(value_store);

    std::string inst_output_deps = inst->getDefinition().getSafeName() + "_output_deps";
    Function *inst_func = env.getModule().getFunctionDecl(inst_output_deps);
    if (inst_func == nullptr) {
      inst_func = env.getModule().makeFunctionDecl(inst_output_deps, makeOutputDepsType(inst->getDefinition(), env.getModule()));
    }

    Value *ret_struct = env.getIRBuilder().CreateCall(inst_func, argument_values, inst->getName() + "_output");
    for (unsigned i = 0; i < sources.size(); i++) {
      Value *struct_elem = env.getIRBuilder().CreateExtractValue(ret_struct, { i });
      ret_values.push_back(struct_elem);
    }
  }

  for (unsigned i = 0; i < ret_values.size(); i++) {
    env.addValue(&sources[i], ret_values[i]);
  }
}

static void makeInstanceStateDeps(const Instance *inst, const SimInfo &defn_info, FunctionEnvironment &env, Value *base_state, Value *inst_offset, Value *value_store)
{
  const SimInfo &inst_info = inst->getDefinition().getSimInfo();
  const InstanceIFace &iface = inst->getIFace();
  if (inst_info.isPrimitive()) {
    return;
  }

  std::vector<Value *> argument_values;

  for (const Source *src : inst_info.getStateSources()) {
    const Sink *sink = iface.getSink(src);
    argument_values.push_back(makeValueReference(sink->getSelect(), env));
  }

  Value *state_ptr = incrementStatePtr(base_state, defn_info.getOffset(inst), env);
  argument_values.push_back(state_ptr);

  inst_offset = env.getIRBuilder().CreateAdd(inst_offset, ConstantInt::get(env.getContext(), APInt(64, defn_info.getInstNum(inst))));
  argument_values.push_back(inst_offset);
  argument_values.push_back(value_store);

  std::string inst_state_deps = inst->getDefinition().getSafeName() + "_state_deps";
  Function *inst_func = env.getModule().getFunctionDecl(inst_state_deps);
  if (inst_func == nullptr) {
    inst_func = env.getModule().makeFunctionDecl(inst_state_deps, makeStateDepsType(inst->getDefinition(), env.getModule()));
  }

  env.getIRBuilder().CreateCall(inst_func, argument_values);
}


std::unique_ptr<Module> MakeOutputDeps(Builder &builder, const Definition &definition)
{
  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName() + "_output_deps");

  const SimInfo &defn_info = definition.getSimInfo();

  FunctionType *od_type = makeOutputDepsType(definition, mod_env);
  FunctionEnvironment output_deps = mod_env.makeFunction(definition.getSafeName() + "_output_deps", od_type);
  output_deps.addBasicBlock("entry");

  const std::vector<const Source *> &sources = defn_info.getOutputSources();
  auto arg = output_deps.getFunction()->arg_begin();
  assert(output_deps.getFunction()->arg_size() == sources.size() + defn_info.isStateful() + 2);

  for (unsigned i = 0; i < sources.size(); i++, arg++) {
    const Source *src = sources[i];

    output_deps.addValue(src, arg);
    arg->setName("self." + src->getName());
  }
  
  Value *state_ptr = nullptr;
  if (defn_info.isStateful()) {
    state_ptr = arg;
    state_ptr->setName("state_ptr");
  }
  arg++;
  Value *inst_offset = arg;
  arg++;
  Value *value_store = arg;

  const std::vector<const Instance *> &output_instances = defn_info.getOutputDeps();
  for (const Instance *inst : output_instances) {
    makeInstanceOutputDeps(inst, defn_info, output_deps, state_ptr, inst_offset, value_store);
  }

  BasicBlock *ret_block = output_deps.addBasicBlock("return", false);
  output_deps.getIRBuilder().CreateBr(ret_block);
  output_deps.setCurBasicBlock(ret_block);

  const std::vector<JITSim::Sink> & sinks = definition.getIFace().getSinks();
  Value *ret_val = UndefValue::get(od_type->getReturnType());

  for (unsigned i = 0; i < sinks.size(); i++ ) {
    Value *ret_part = makeValueReference(sinks[i].getSelect(), output_deps);
    ret_val = output_deps.getIRBuilder().CreateInsertValue(ret_val, ret_part, { i });
  }

  output_deps.getIRBuilder().CreateRet(ret_val);

  assert(!output_deps.verify());
  
  return mod_env.returnModule();
}

std::unique_ptr<Module> MakeStateDeps(Builder &builder, const Definition &definition)
{
  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName() + "_state_deps");

  const SimInfo &defn_info = definition.getSimInfo();

  FunctionType *sd_type = makeStateDepsType(definition, mod_env);
  FunctionEnvironment state_deps = mod_env.makeFunction(definition.getSafeName() + "_state_deps", sd_type);
  state_deps.addBasicBlock("entry");

  const std::vector<const Source *> & sources = defn_info.getStateSources();
  auto arg = state_deps.getFunction()->arg_begin();
  assert(state_deps.getFunction()->arg_size() == sources.size() + 3);

  for (unsigned i = 0; i < sources.size(); i++, arg++) {
    const Source *src = sources[i];

    state_deps.addValue(src, arg);
    arg->setName("self." + src->getName());
  }

  Value *state_ptr = arg;
  state_ptr->setName("state_ptr");
  arg++;
  Value *inst_offset = arg;
  arg++;
  Value *value_store = arg;

  for (const Instance *inst : defn_info.getStateDeps()) {
    makeInstanceOutputDeps(inst, defn_info, state_deps, state_ptr, inst_offset, value_store);
  }

  for (const Instance *inst : defn_info.getStatefulInstances()) {
    makeInstanceStateDeps(inst, defn_info, state_deps, state_ptr, inst_offset, value_store);
  }

  BasicBlock *ret_block = state_deps.addBasicBlock("return", false);
  state_deps.getIRBuilder().CreateBr(ret_block);
  state_deps.setCurBasicBlock(ret_block);

  state_deps.getIRBuilder().CreateRetVoid();
  assert(!state_deps.verify());
  assert(!mod_env.verify());

  return mod_env.returnModule();
}

std::unique_ptr<Module> MakeComputeOutputWrapper(Builder &builder, const Definition &defn)
{
  ModuleEnvironment mod_env = builder.makeModule(defn.getSafeName() + "_compute_output_wrapper");

  const std::vector<const Source *> & sources = defn.getSimInfo().getOutputSources();
  const std::vector<Sink> & sinks = defn.getIFace().getSinks();

  FunctionType *wrapper_type =
    FunctionType::get(Type::getVoidTy(mod_env.getContext()),
                      {ConstructStructType(sources, mod_env.getContext(), "co_wrapper_input")->getPointerTo(),
                       ConstructStructType(sinks, mod_env.getContext(), "co_wrapper_output")->getPointerTo(),
                       Type::getInt8PtrTy(mod_env.getContext())}, false);

  FunctionEnvironment func = mod_env.makeFunction("compute_output", wrapper_type);
  func.addBasicBlock("entry");

  Value *inputs = func.getFunction()->arg_begin();
  Value *outputs = func.getFunction()->arg_begin() + 1;
  Value *state = func.getFunction()->arg_begin() + 2;

  FunctionType *co_type = makeComputeOutputType(defn, mod_env);
  Function *underlying = mod_env.makeFunctionDecl(defn.getSafeName() + "_compute_output", co_type);

  std::vector<Value *> args;
  for (unsigned i = 0; i < sources.size(); i++) {
    Value *arg = func.getIRBuilder().CreateStructGEP(inputs->getType()->getPointerElementType(), inputs, i);
    arg = func.getIRBuilder().CreateLoad(arg);
    args.push_back(arg);
  }
  args.push_back(state);

  Value *output_struct = func.getIRBuilder().CreateCall(underlying, args);

  for (unsigned i = 0; i < sinks.size(); i++) {
    Value *val = func.getIRBuilder().CreateExtractValue(output_struct, { i });
    Value *addr = func.getIRBuilder().CreateStructGEP(outputs->getType()->getPointerElementType(), outputs, i);
    func.getIRBuilder().CreateStore(val, addr);
  }

  func.getIRBuilder().CreateRetVoid();

  func.verify();

  return mod_env.returnModule();
}

std::unique_ptr<Module> MakeUpdateStateWrapper(Builder &builder, const Definition &defn)
{
  ModuleEnvironment mod_env = builder.makeModule(defn.getSafeName() + "_update_state_wrapper");

  const std::vector<const Source *> & sources = defn.getSimInfo().getStateSources();

  FunctionType *wrapper_type =
    FunctionType::get(Type::getVoidTy(mod_env.getContext()),
                      {ConstructStructType(sources, mod_env.getContext(), "us_wrapper_input")->getPointerTo(), 
                       Type::getInt8PtrTy(mod_env.getContext())}, false);

  FunctionEnvironment func = mod_env.makeFunction("update_state", wrapper_type);
  func.addBasicBlock("entry");

  Value *inputs = func.getFunction()->arg_begin();
  Value *state = func.getFunction()->arg_begin() + 1;

  FunctionType *us_type = makeUpdateStateType(defn, mod_env);
  Function *underlying = mod_env.makeFunctionDecl(defn.getSafeName() + "_update_state", us_type);

  std::vector<Value *> args;
  for (unsigned i = 0; i < sources.size(); i++) {
    Value *arg = func.getIRBuilder().CreateStructGEP(inputs->getType()->getPointerElementType(), inputs, i);
    arg = func.getIRBuilder().CreateLoad(arg);
    args.push_back(arg);
  }
  args.push_back(state);

  func.getIRBuilder().CreateCall(underlying, args);

  func.getIRBuilder().CreateRetVoid();
  func.verify();

  return mod_env.returnModule();
}

std::unique_ptr<Module> MakeGetValuesWrapper(Builder &builder, const Definition &defn)
{
  ModuleEnvironment mod_env = builder.makeModule(defn.getSafeName() + "_get_values_wrapper");

  const std::vector<Source> &sources = defn.getIFace().getSources();

  FunctionType *wrapper_type =
    FunctionType::get(Type::getVoidTy(mod_env.getContext()),
                      {ConstructStructType(sources, mod_env.getContext(), "gv_wrapper_input")->getPointerTo(),
                       Type::getInt8PtrTy(mod_env.getContext()),
                       Type::getInt8PtrTy(mod_env.getContext())}, false);

  FunctionEnvironment func = mod_env.makeFunction("get_values", wrapper_type);
  func.addBasicBlock("entry");

  Value *inputs = func.getFunction()->arg_begin();
  Value *value_store = func.getFunction()->arg_begin() + 1;
  Value *state = func.getFunction()->arg_begin() + 2;

  const std::vector<const Source *> &output_dep_sources = defn.getSimInfo().getOutputSources();
  const std::vector<const Source *> &state_dep_sources = defn.getSimInfo().getStateSources();
  std::vector<Value *> output_deps_args;
  std::vector<Value *> state_deps_args;
  for (unsigned i = 0, o_idx = 0, s_idx = 0; i < sources.size(); i++) {
    const Source *cur_src = &sources[i];
    const Source *cur_o_src = o_idx < output_dep_sources.size() ? output_dep_sources[o_idx] : nullptr;
    const Source *cur_s_src = s_idx < state_dep_sources.size() ? state_dep_sources[s_idx] : nullptr;
    bool o_match = cur_src == cur_o_src;
    bool s_match = cur_src == cur_s_src;
    if (o_match || s_match) {
      Value *arg = func.getIRBuilder().CreateStructGEP(inputs->getType()->getPointerElementType(), inputs, i);
      arg = func.getIRBuilder().CreateLoad(arg);
      if (o_match) {
        output_deps_args.push_back(arg);
        o_idx++;
      }
      if (s_match) {
        state_deps_args.push_back(arg);
        s_idx++;
      }
    }
  }

  Value *zero = ConstantInt::get(mod_env.getContext(), APInt(64, 0));

  output_deps_args.push_back(state);
  output_deps_args.push_back(zero);
  output_deps_args.push_back(value_store);

  state_deps_args.push_back(state);
  state_deps_args.push_back(zero);
  state_deps_args.push_back(value_store);

  FunctionType *sd_type = makeStateDepsType(defn, mod_env);
  Function *sd_underlying = mod_env.makeFunctionDecl(defn.getSafeName() + "_state_deps", sd_type);
  FunctionType *od_type = makeOutputDepsType(defn, mod_env);
  Function *od_underlying = mod_env.makeFunctionDecl(defn.getSafeName() + "_output_deps", od_type);

  func.getIRBuilder().CreateCall(od_underlying, output_deps_args);
  func.getIRBuilder().CreateCall(sd_underlying, state_deps_args);

  func.getIRBuilder().CreateRetVoid();
  func.verify();
  return mod_env.returnModule();
}

}
