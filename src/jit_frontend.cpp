#include <jitsim/jit_frontend.hpp>

namespace JITSim {

using namespace std;
LLVMStruct::LLVMStruct(const StructLayout *l, llvm::ArrayRef<std::pair<std::string, int>> idxs)

void LLVMStruct::setInput(const string &name, llvm::APInt val)
{
  auto iter = member_indices.find(name);
  if (iter == iter.end()) {
    return;
  }
  int idx = iter->second;
  uint64_t offset = layout->getElementOffset(idx);

  uint8_t *ptr = data.data();
  ptr += offset;
  memcpy(
}

JITFrontend::JITFrontend(const Circuit &circuit, const Definition &top)
  : target_machine(EngineBuilder().selectTarget()),
    data_layout(target_machine->createDataLayout()),
    jit(target_machine, data_layout),
    builder(data_layout, *target_machine),
    co_in(top.getSimInfo()),
    co_out(top.getSimInfo()),
    us_in(top.getSimInfo()),
    jit_modules(),
    state(top.getSimInfo().allocateState()),
    compute_output_ptr(nullptr),
    update_state_ptr(nullptr)
{
  vector<ModuleEnvironment> ir_modules = ModulesForCircuit(builder, circuit);
  for (const ModuleEnvironment &ir_mod : ir_modules) {
    cout << "\n=================\n\n";
    cout << mod.getIRStr();
  }
  cout << "\n=================\n";

  for (ModuleEnvironment &mod : modules) {
    jit_modules.push_back(jit.addModule(mod.returnModule()));
  }

  ModuleEnvironment wrapper_mod = GenerateWrapper(builder, top);
  handles.push_back(jit.addModule(wrapper_mod.returnModule()));

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
  co_in.setInput(name, val);
  us_in.setInput(name, val);
}

void JITFrontend::updateState()
{
  update_state_ptr(co_in.getData(), state.data());
}

const LLVMStruct & computeOutput()
{
  compute_output_ptr(co_in.getData(), co_out.getData(), state.data());
  return co_out;
}

}
