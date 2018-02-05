#include <jitsim/jit_frontend.hpp>
#include "utils.hpp"
#include "llvm_utils.hpp"

#include <llvm/IR/ValueSymbolTable.h>

namespace JITSim {

using namespace std;

static bool isPrimitive(const Definition &definition)
{
  const SimInfo &siminfo = definition.getSimInfo();
  return siminfo.isPrimitive();
}

template <typename T>
LLVMStruct::LLVMStruct(const vector<T> &members,
                       const llvm::DataLayout &data_layout,
                       llvm::LLVMContext &context)
  : type(ConstructStructType(members, context)),
    layout(data_layout.getStructLayout(type)),
    member_indices(),
    data(layout->getSizeInBytes(), 0)
{
  for (unsigned i = 0; i < members.size(); i++) {
    const auto &m = condDeref(members[i]);
    member_indices[m.getName()] = i;
  }
}

uint8_t * LLVMStruct::getMemberAddr(int idx)
{
  uint8_t *ptr = data.data();
  uint64_t offset = layout->getElementOffset(idx);

  return ptr + offset;
}

const uint8_t * LLVMStruct::getMemberAddr(int idx) const
{
  const uint8_t *ptr = data.data();
  uint64_t offset = layout->getElementOffset(idx);

  return ptr + offset;
}

int LLVMStruct::getMemberBits(int idx) const
{
  llvm::Type *elemty = type->getElementType(idx);
  return elemty->getIntegerBitWidth();
}

void LLVMStruct::setMember(const string &name, llvm::APInt val)
{
  auto iter = member_indices.find(name);
  if (iter == member_indices.end()) {
    return;
  }
  int idx = iter->second;
  uint8_t *ptr = getMemberAddr(idx);
  memcpy(ptr, val.getRawData(), getNumBytes(getMemberBits(idx)));
}

llvm::APInt LLVMStruct::getValue(int idx) const 
{
  const uint8_t *ptr = getMemberAddr(idx);
  int bits = getMemberBits(idx);
  int bytes = bits / 8;
  if (bits % 8 != 0) {
    bytes++;
  }
  int num64s = bits / 64;
  if (bits % 64 != 0) {
    num64s++;
  }
  vector<uint64_t> safe_arr(num64s, 0);
  memcpy(safe_arr.data(), ptr, bytes);

  return llvm::APInt(bits, llvm::ArrayRef<uint64_t>(safe_arr.data(), num64s));
}

llvm::APInt LLVMStruct::getValue(const string &name) const
{
  int idx = member_indices.find(name)->second;
  return getValue(idx);
}

void LLVMStruct::dump() const
{
  for (const auto &name_pair : member_indices) {
    cout << name_pair.first << ": " << getValue(name_pair.second).toString(10, false) << endl;
  }
}

void JITFrontend::addDefinitionFunctions(const Definition &defn)
{
  jit.addLazyFunction(defn.getSafeName() + "_update_state", [this, &defn]() {
    ModuleEnvironment env = MakeUpdateState(builder, defn);

    return env.getModule();
  });

  jit.addLazyFunction(defn.getSafeName() + "_compute_output", [this, &defn]() {
    ModuleEnvironment env = MakeComputeOutput(builder, defn);

    return env.getModule();
  });

  jit.addLazyFunction(defn.getSafeName() + "_state_deps", [this, &defn]() {
    ModuleEnvironment env = MakeStateDeps(builder, defn);
    debug_modules.emplace(defn.getSafeName() + "_state_deps", move(env));

    return env.getModule();
  });

  jit.addLazyFunction(defn.getSafeName() + "_output_deps", [this, &defn]() {
    ModuleEnvironment env = MakeOutputDeps(builder, defn);
    debug_modules.emplace(defn.getSafeName() + "_output_deps", move(env));

    return env.getModule();
  });
}

void JITFrontend::addWrappers(const Definition &top)
{
  jit.addLazyFunction("update_state", [this, &top]() {
    return MakeUpdateStateWrapper(builder, top).getModule();
  });

  jit.addLazyFunction("compute_output", [this, &top]() {
    return MakeComputeOutputWrapper(builder, top).getModule();
  });

  jit.addLazyFunction("get_values", [this, &top]() {
    return MakeGetValuesWrapper(builder, top).getModule();
  });
}

JITFrontend::JITFrontend(const Circuit &circuit, const Definition &top_)
  : target_machine(llvm::EngineBuilder().selectTarget()),
    data_layout(target_machine->createDataLayout()),
    builder(data_layout, *target_machine),
    jit(*target_machine, data_layout),
    co_in(top_.getSimInfo().getOutputSources(), data_layout, builder.getContext()),
    co_out(top_.getIFace().getSinks(), data_layout, builder.getContext()),
    us_in(top_.getSimInfo().getStateSources(), data_layout, builder.getContext()),
    gv_in(top_.getIFace().getSources(), data_layout, builder.getContext()),
    state(top_.getSimInfo().allocateState()),
    compute_output_ptr(nullptr),
    update_state_ptr(nullptr),
    top(&top_)
{
  for (const Definition &defn : circuit.getDefinitions()) {
    if (!isPrimitive(defn)) {
      addDefinitionFunctions(defn);
    } else {
      // FIXME handle primitives that want to provide function definitions
    }
  }
  addWrappers(top_);

  compute_output_ptr = (WrapperComputeOutputFn)jit.getSymbolAddress("compute_output");
  update_state_ptr = (WrapperUpdateStateFn)jit.getSymbolAddress("update_state");
  get_values_ptr = (WrapperGetValuesFn)jit.getSymbolAddress("get_values");

  assert(compute_output_ptr && update_state_ptr);
}

JITFrontend::JITFrontend(const Circuit &circuit)
  : JITFrontend(circuit, circuit.getTopDefinition())
{}

void JITFrontend::setInput(const std::string &name, uint64_t val)
{
  setInput(name, llvm::APInt(64, val));
}

void JITFrontend::setInput(const std::string &name, llvm::APInt val)
{
  co_in.setMember(name, val);
  us_in.setMember(name, val);
  gv_in.setMember(name, val);
}

void JITFrontend::updateState()
{
  update_state_ptr(us_in.getData(), state.data());
}

const LLVMStruct & JITFrontend::computeOutput()
{
  compute_output_ptr(co_in.getData(), co_out.getData(), state.data());
  return co_out;
}

static tuple<const Definition *, const Instance *, unsigned> getDefnAndInst(const Definition *top, const vector<string> &inst_names)
{
  const Definition *cur_defn = top;
  unsigned inst_num = 0;
  for (unsigned i = 0; i < inst_names.size() - 1; i++) {
    const string &inst_name = inst_names[i];
    const SimInfo &sim_info = cur_defn->getSimInfo();
    const Instance &inst = cur_defn->getInstance(inst_name);
    cur_defn = &inst.getDefinition();
    inst_num += sim_info.getInstNum(&inst);
  }

  const Instance *inst = &cur_defn->getInstance(inst_names.back());

  return make_tuple(cur_defn, inst, inst_num);
}

vector<uint8_t> JITFrontend::allocateDebugStorage(const Instance *inst, const string &input)
{
  int num_bits = 0;
  const IFace &iface = inst->getIFace();
  if (iface.hasSource(input)) {
    const Source *src = iface.getSource(input);
    num_bits = src->getWidth();
  } else if (iface.hasSink(input)) {
    const Sink *sink = iface.getSink(input);
    num_bits = sink->getWidth();
  }

  unsigned num_bytes = num_bits / 8;
  if (num_bits % 8 != 0) {
    num_bytes++;
  }
  
  assert(num_bytes);
  return vector<uint8_t>(num_bytes, 0);
}

llvm::APInt JITFrontend::getValue(const vector<string> &inst_names, const string &input)
{
  const Definition *defn;
  const Instance *inst;
  unsigned inst_num;
  tie(defn, inst, inst_num) = getDefnAndInst(top, inst_names);
  vector<uint8_t> debug_store = allocateDebugStorage(inst, input);
  assert(debug_store.data() && "Data store is null?");
  const SimInfo &defn_info = defn->getSimInfo();

  bool in_output_deps = defn_info.isOutputDep(inst);
  assert(in_output_deps || defn_info.isStateDep(inst));

  string mod_name;
  if (in_output_deps) {
    mod_name = defn->getSafeName() + "_output_deps";
  } else {
    mod_name = defn->getSafeName() + "_state_deps";
  }
  if (jit.removeModule(mod_name)) {
    /* If the module was removed, add a new callback to use the already generated IR */
    assert(debug_modules.count(mod_name));

    jit.addLazyFunction(mod_name, [this, mod_name]() {
      ModuleEnvironment &env = debug_modules.find(mod_name)->second;
      return env.getModule();
    });
  }
  jit.addDebugTransform(mod_name, [this, inst, mod_name, input, inst_num, debug_store, defn](std::shared_ptr<llvm::Module> module) {
    ModuleEnvironment &env = debug_modules.find(mod_name)->second;

    llvm::Value *llvm_val = nullptr;
    const IFace &inst_iface = inst->getIFace();
    if (inst_iface.hasSource(input)) {
      const Source *src = inst_iface.getSource(input);
      llvm_val = env.lookupValue(src);
    } else if (inst_iface.hasSink(input)) {
      const Sink *sink = inst_iface.getSink(input);
      llvm_val = env.lookupValue(sink);
    } else {
      assert(false);
    }
    assert(llvm_val && "Can't find queried value");

    auto func = module->getFunction(mod_name);
    assert(func && "unable to find function to modify");
    auto symtab = func->getValueSymbolTable();
    assert(symtab && "unable to find function symbol table");

    llvm::Value *inst_offset = symtab->lookup("inst_offset");
    assert(inst_offset && "Can't find inst_offset param");
    llvm::IRBuilder<> ir_builder(builder.getContext());

    llvm::BasicBlock *last_bb = &func->back();
    llvm::BasicBlock *debug_block = llvm::BasicBlock::Create(builder.getContext(), "debug_block", func);

    // Rewrite every predecessor of the return block to instead jump to the debug block
    vector<llvm::Instruction *> terms;
    for (auto pred : llvm::predecessors(last_bb)) {
      llvm::Instruction *term = pred->getTerminator();
      terms.push_back(term); // Can't erase term here since it will mess up the predecessor iterator
      ir_builder.SetInsertPoint(term);
      ir_builder.CreateBr(debug_block);
    }

    for (auto term : terms) {
      term->eraseFromParent();
    }

    ir_builder.SetInsertPoint(debug_block);
    llvm::Value *inst_eq = ir_builder.CreateICmpEQ(inst_offset, llvm::ConstantInt::get(builder.getContext(), llvm::APInt(64, inst_num)));
    llvm::BasicBlock *val_save = llvm::BasicBlock::Create(builder.getContext(), "inst_match", func);
    ir_builder.CreateCondBr(inst_eq, val_save, last_bb);
    ir_builder.SetInsertPoint(val_save);


    llvm::Value *addr = llvm::Constant::getIntegerValue(llvm_val->getType()->getPointerTo(), llvm::APInt(64, (uint64_t)debug_store.data()));
    ir_builder.CreateStore(llvm_val, addr);
    ir_builder.CreateBr(last_bb);

    llvm::outs() << "========== Modified " << defn->getName() << " ===========\n";
    llvm::outs() << *module;
    llvm::outs() << "=====================================\n";
    if (llvm::verifyModule(*module)) {
      llvm::errs() << "Modified module has errors\n";
    }

    return module;
  });

  get_values_ptr(gv_in.getData(), state.data());

  int num64s = debug_store.size() / 8;
  if (debug_store.size() % 8 != 0) {
    num64s++;
  }
  vector<uint64_t> safe_arr(num64s, 0);
  memcpy(safe_arr.data(), debug_store.data(), debug_store.size());

  return llvm::APInt(debug_store.size()*8, llvm::ArrayRef<uint64_t>(safe_arr.data(), num64s));
}

void JITFrontend::dumpIR()
{
  jit.precompileDumpIR();
}

}
