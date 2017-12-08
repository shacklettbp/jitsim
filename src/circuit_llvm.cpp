#include <jitsim/circuit_llvm.hpp>
#include "llvm_utils.hpp"

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
    std::string inst_comp_sources = getComputeOutputName(inst->getDefinition());
    Function *inst_func = env.getModule().getFunctionDecl(inst_comp_sources);
    if (inst_func == nullptr) {
      inst_func = env.getModule().makeFunctionDecl(inst_comp_sources, makeComputeOutputType(inst->getDefinition(), env.getModule()));
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

  std::vector<Value *> ret_values;
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

static void makeComputeOutputDefn(const Definition &definition, ModuleEnvironment &mod_env)
{
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
}

static void makeUpdateStateDefn(const Definition &definition, ModuleEnvironment &mod_env)
{
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
}

ModuleEnvironment ModuleForDefinition(Builder &builder, const Definition &definition)
{
  const SimInfo &siminfo = definition.getSimInfo();
  if (siminfo.isPrimitive()) {
    return ModuleForPrimitive(builder, definition, siminfo.getPrimitive());
  }

  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName());

  makeComputeOutputDefn(definition, mod_env);
  makeDebugOutputDepsDefn(definition, mod_env);

  if (siminfo.isStateful()) {
    makeUpdateStateDefn(definition, mod_env);
    makeDebugStateDepsDefn(definition, mod_env);
  }

  assert(!mod_env.verify());

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

static void makeComputeOutputWrapper(const Definition &defn, ModuleEnvironment &env)
{
  const std::vector<const Source *> & sources = defn.getSimInfo().getOutputSources();
  const std::vector<Sink> & sinks = defn.getIFace().getSinks();

  FunctionType *wrapper_type =
    FunctionType::get(Type::getVoidTy(env.getContext()),
                      {ConstructStructType(sources, env.getContext(), "co_wrapper_input")->getPointerTo(),
                       ConstructStructType(sinks, env.getContext(), "co_wrapper_output")->getPointerTo(),
                       Type::getInt8PtrTy(env.getContext())}, false);

  FunctionEnvironment func = env.makeFunction("compute_output", wrapper_type);
  func.addBasicBlock("entry");

  Value *inputs = func.getFunction()->arg_begin();
  Value *outputs = func.getFunction()->arg_begin() + 1;
  Value *state = func.getFunction()->arg_begin() + 2;

  FunctionType *co_type = makeComputeOutputType(defn, env);
  Function *underlying = env.makeFunctionDecl(defn.getSafeName() + "_compute_output", co_type);

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
}

static void makeUpdateStateWrapper(const Definition &defn, ModuleEnvironment &env)
{
  const std::vector<const Source *> & sources = defn.getSimInfo().getStateSources();

  FunctionType *wrapper_type =
    FunctionType::get(Type::getVoidTy(env.getContext()),
                      {ConstructStructType(sources, env.getContext(), "us_wrapper_input")->getPointerTo(), 
                       Type::getInt8PtrTy(env.getContext())}, false);

  FunctionEnvironment func = env.makeFunction("update_state", wrapper_type);
  func.addBasicBlock("entry");

  Value *inputs = func.getFunction()->arg_begin();
  Value *state = func.getFunction()->arg_begin() + 1;

  FunctionType *us_type = makeUpdateStateType(defn, env);
  Function *underlying = env.makeFunctionDecl(defn.getSafeName() + "_update_state", us_type);

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
}

ModuleEnvironment GenerateWrapper(Builder &builder, const Definition &definition)
{
  ModuleEnvironment mod_env = builder.makeModule(definition.getSafeName() + "_wrapper");

  makeComputeOutputWrapper(definition, mod_env);
  makeUpdateStateWrapper(definition, mod_env);

  mod_env.verify();
  return mod_env;
}

}
