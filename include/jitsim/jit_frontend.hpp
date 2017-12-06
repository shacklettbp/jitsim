#ifndef SIMJIT_JIT_FRONTEND_HPP_INCLUDED
#define SIMJIT_JIT_FRONTEND_HPP_INCLUDED

#include <jitsim/JIT.hpp>
#include <jitsim/builder.hpp>
#include <jitsim/circuit.hpp>

namespace JITSim {

class LLVMStruct {
private:
  const llvm::StructLayout *layout;
  std::unordered_map<std::string, int> member_indices;
  vector<uint8_t> data;
public:
  LLVMStruct(const StructLayout *l, llvm::ArrayRef<std::pair<std::string, int>> idxs);

  void setInput(const std::string &name, llvm::APInt val);

  uint8_t *getData() { return data.data(); }
};

class JITFrontend {
private:
  std::unique_ptr<llvm::TargetMachine> target_machine;
  const llvm::DataLayout data_layout;

  JIT jit;
  Builder builder;

  LLVMStruct co_in;
  LLVMStruct co_out;
  LLVMStruct us_in;

  vector<JIT::ModuleHandle> jit_modules;
  vector<uint8_t> state;

  WrapperComputeOutputFn compute_output_ptr;
  WrapperUpdateStateFn update_state_ptr;
  JITFrontend(const Circuit &circuit, const SimInfo &top_info);
public:
  JITFrontend(const Circuit &circuit);
  ~JITFrontend();

  void setInput(const std::string &name, uint64_t val);
  void setInput(const std::string &name, llvm::APInt val);

  const vector<uint8_t> & getState() const { return state; }

  void updateState();
  const LLVMStruct & computeOutput();
};

};

#endif
