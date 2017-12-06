#ifndef SIMJIT_JIT_FRONTEND_HPP_INCLUDED
#define SIMJIT_JIT_FRONTEND_HPP_INCLUDED

#include <jitsim/JIT.hpp>
#include <jitsim/builder.hpp>
#include <jitsim/circuit.hpp>
#include <jitsim/circuit_llvm.hpp>

namespace JITSim {

class LLVMStruct {
private:
  llvm::StructType *type;
  const llvm::StructLayout *layout;
  std::unordered_map<std::string, int> member_indices;
  std::vector<uint8_t> data;

  uint8_t *getMemberAddr(int idx);
  int getMemberBits(int idx) const;

public:
  template <typename T>
  LLVMStruct(const std::vector<T> &members, const llvm::DataLayout &data_layout,
             llvm::LLVMContext &context);

  void setMember(const std::string &name, llvm::APInt val);

  llvm::APInt getValue(const std::string &name);

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

  std::vector<JIT::ModuleHandle> jit_modules;
  std::vector<uint8_t> state;

  WrapperComputeOutputFn compute_output_ptr;
  WrapperUpdateStateFn update_state_ptr;
  JITFrontend(const Circuit &circuit, const Definition &top);
public:
  JITFrontend(const Circuit &circuit);
  ~JITFrontend();

  void setInput(const std::string &name, uint64_t val);
  void setInput(const std::string &name, llvm::APInt val);

  const std::vector<uint8_t> & getState() const { return state; }

  void updateState();
  const LLVMStruct & computeOutput();
};

}

#endif
