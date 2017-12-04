#ifndef JITSIM_PRIMITIVE_HPP_INCLUDED
#define JITSIM_PRIMITIVE_HPP_INCLUDED

#include <functional>
#include <unordered_set>
#include <jitsim/builder.hpp>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>

namespace JITSim {

class Instance;

struct Primitive {
public:
  bool is_stateful;
  bool has_definition;
  unsigned int num_state_bytes;
  std::unordered_set<std::string> state_deps;
  std::unordered_set<std::string> output_deps;
  
  using ComputeOutputGen = std::function<std::vector<llvm::Value *> (
      FunctionEnvironment &env, const std::vector<llvm::Value *> &args, const Instance &inst
      )>;
  using UpdateStateGen = std::function<void (
      FunctionEnvironment &env, const std::vector<llvm::Value *> &args, const Instance &inst
      )>;
  using ModuleGen = std::function<void (ModuleEnvironment &env)>;

  ComputeOutputGen make_compute_output;
  UpdateStateGen make_update_state;
  ModuleGen make_def;

  Primitive(bool is_stateful_,
            unsigned int num_state_bytes_,
            const std::unordered_set<std::string> & state_deps_,
            const std::unordered_set<std::string> & output_deps_,
            ComputeOutputGen make_compute_output_,
            UpdateStateGen make_update_state_,
            ModuleGen make_def_)
    : is_stateful(is_stateful_),
      has_definition(true),
      num_state_bytes(num_state_bytes_),
      state_deps(state_deps_),
      output_deps(output_deps_),
      make_compute_output(make_compute_output_),
      make_update_state(make_update_state_),
      make_def(make_def_)
  {
  }
  
  Primitive(bool is_stateful_,
            unsigned int num_state_bytes_,
            const std::unordered_set<std::string> & state_deps_,
            const std::unordered_set<std::string> & output_deps_,
            ComputeOutputGen make_compute_output_,
            UpdateStateGen make_update_state_)
    : is_stateful(is_stateful_),
      has_definition(false),
      num_state_bytes(num_state_bytes_),
      state_deps(state_deps_),
      output_deps(output_deps_),
      make_compute_output(make_compute_output_),
      make_update_state(make_update_state_),
      make_def()
  {
  }

  Primitive(ComputeOutputGen make_compute_output_)
    : is_stateful(false),
      has_definition(false),
      num_state_bytes(0),
      state_deps(),
      output_deps(),
      make_compute_output(make_compute_output_),
      make_update_state(),
      make_def()
  {
  }
};

}

#endif
