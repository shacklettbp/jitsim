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
  const uint8_t *getMemberAddr(int idx) const;
  int getMemberBits(int idx) const;

public:
  template <typename T>
  LLVMStruct(const std::vector<T> &members, const llvm::DataLayout &data_layout,
             llvm::LLVMContext &context);

  void setMember(const std::string &name, llvm::APInt val);

  llvm::APInt getValue(int idx) const;
  llvm::APInt getValue(const std::string &name) const;

  uint8_t *getData() { return data.data(); }

  void dump() const;
};

class JITFrontend {
private:
  std::unique_ptr<llvm::TargetMachine> target_machine;
  const llvm::DataLayout data_layout;

  Builder builder;
  JIT jit;
  std::unordered_map<std::string, ModuleEnvironment> debug_modules;
  std::unordered_map<std::string, llvm::ValueToValueMapTy> debug_clone_map;

  LLVMStruct co_in;
  LLVMStruct co_out;
  LLVMStruct us_in;
  LLVMStruct gv_in;

  std::vector<uint8_t> state;

  using WrapperUpdateStateFn = void (*)(const uint8_t *input, uint8_t *state);
  using WrapperComputeOutputFn = void (*)(const uint8_t *input, uint8_t *output, uint8_t *state);
  using WrapperGetValuesFn = void (*)(const uint8_t *input, uint8_t *state);

  WrapperComputeOutputFn compute_output_ptr;
  WrapperUpdateStateFn update_state_ptr;
  WrapperGetValuesFn get_values_ptr;

  const Definition *top;

  void addDefinitionFunctions(const Definition &defn);
  void addWrappers(const Definition &top);
  std::vector<uint8_t> allocateDebugStorage(const Instance *inst, const std::string &input);

  JITFrontend(const Circuit &circuit, const Definition &top);
public:
  JITFrontend(const Circuit &circuit);

  void setInput(const std::string &name, uint64_t val);
  void setInput(const std::string &name, llvm::APInt val);

  const std::vector<uint8_t> & getState() const { return state; }

  void updateState();
  const LLVMStruct & computeOutput();

  llvm::APInt getValue(const std::vector<std::string> &inst_names, const std::string &input);

  void dumpIR();
};

}

#endif
