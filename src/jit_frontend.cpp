#include <jitsim/jit_frontend.hpp>
#include "utils.hpp"
#include "llvm_utils.hpp"

namespace JITSim {

using namespace std;

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

llvm::APInt LLVMStruct::getValue(const string &name)
{
  int idx = member_indices.find(name)->second;
  uint8_t *ptr = getMemberAddr(idx);
  int bits = getMemberBits(idx);
  int num64s = bits / 64;
  if (bits % 64 != 0) {
    num64s++;
  }


  return llvm::APInt(bits, llvm::ArrayRef<uint64_t>((uint64_t *)ptr, num64s));
}

JITFrontend::JITFrontend(const Circuit &circuit, const Definition &top)
  : target_machine(llvm::EngineBuilder().selectTarget()),
    data_layout(target_machine->createDataLayout()),
    jit(*target_machine, data_layout),
    builder(data_layout, *target_machine),
    co_in(top.getSimInfo().getOutputSources(), data_layout, builder.getContext()),
    co_out(top.getIFace().getSinks(), data_layout, builder.getContext()),
    us_in(top.getSimInfo().getStateSources(), data_layout, builder.getContext()),
    jit_modules(),
    state(top.getSimInfo().allocateState()),
    compute_output_ptr(nullptr),
    update_state_ptr(nullptr)
{
  vector<ModuleEnvironment> ir_modules = ModulesForCircuit(builder, circuit);
  ir_modules.emplace_back(GenerateWrapper(builder, top));

  for (const ModuleEnvironment &ir_mod : ir_modules) {
    cout << "\n=================\n\n";
    cout << ir_mod.getIRStr();
  }
  cout << "\n=================\n";

  for (ModuleEnvironment &ir_mod : ir_modules) {
    jit_modules.push_back(jit.addModule(ir_mod.returnModule()));
  }

  compute_output_ptr = (WrapperComputeOutputFn)jit.getSymbolAddress("compute_output");
  update_state_ptr = (WrapperUpdateStateFn)jit.getSymbolAddress("update_state");
}

JITFrontend::JITFrontend(const Circuit &circuit)
  : JITFrontend(circuit, circuit.getTopDefinition())
{}

JITFrontend::~JITFrontend()
{
  for (auto &handle : jit_modules) {
    jit.removeModule(handle);
  }
}

void JITFrontend::setInput(const std::string &name, uint64_t val)
{
  setInput(name, llvm::APInt(64, val));
}

void JITFrontend::setInput(const std::string &name, llvm::APInt val)
{
  co_in.setMember(name, val);
  us_in.setMember(name, val);
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

}
