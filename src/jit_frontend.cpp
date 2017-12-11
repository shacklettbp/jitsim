#include <jitsim/jit_frontend.hpp>
#include "utils.hpp"
#include "llvm_utils.hpp"

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
  int num64s = bits / 64;
  if (bits % 64 != 0) {
    num64s++;
  }

  return llvm::APInt(bits, llvm::ArrayRef<uint64_t>((const uint64_t *)ptr, num64s));
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
    return MakeUpdateState(builder, defn);
  });

  jit.addLazyFunction(defn.getSafeName() + "_compute_output", [this, &defn]() {
    return MakeComputeOutput(builder, defn);
  });
}

void JITFrontend::addWrappers(const Definition &top)
{
  jit.addLazyFunction("update_state", [this, &top]() {
    return MakeUpdateStateWrapper(builder, top);
  });

  jit.addLazyFunction("compute_output", [this, &top]() {
    return MakeComputeOutputWrapper(builder, top);
  });

  jit.addLazyFunction("get_values", [this, &top]() {
    return MakeGetValuesWrapper(builder, top);
  });
}

JITFrontend::JITFrontend(const Circuit &circuit, const Definition &top)
  : target_machine(llvm::EngineBuilder().selectTarget()),
    data_layout(target_machine->createDataLayout()),
    jit(*target_machine, data_layout),
    builder(data_layout, *target_machine),
    co_in(top.getSimInfo().getOutputSources(), data_layout, builder.getContext()),
    co_out(top.getIFace().getSinks(), data_layout, builder.getContext()),
    us_in(top.getSimInfo().getStateSources(), data_layout, builder.getContext()),
    gv_in(top.getIFace().getSources(), data_layout, builder.getContext()),
    state(top.getSimInfo().allocateState()),
    compute_output_ptr(nullptr),
    update_state_ptr(nullptr)
{
  for (const Definition &defn : circuit.getDefinitions()) {
    if (!isPrimitive(defn)) {
      addDefinitionFunctions(defn);
    } else {
      // FIXME handle primitives that want to provide function definitions
    }
  }
  addWrappers(top);

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
  update_state_ptr(co_in.getData(), state.data());
}

const LLVMStruct & JITFrontend::computeOutput()
{
  compute_output_ptr(co_in.getData(), co_out.getData(), state.data());
  return co_out;
}

void JITFrontend::dumpIR()
{
  jit.precompileDumpIR();
}

}
