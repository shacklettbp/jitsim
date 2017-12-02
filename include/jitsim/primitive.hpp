#ifndef JITSIM_PRIMITIVE_HPP_INCLUDED
#define JITSIM_PRIMITIVE_HPP_INCLUDED

#include <functional>
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
  std::function<std::vector<llvm::Value *> (
      FunctionEnvironment &env, const std::vector<llvm::Value *> &args, const Instance &inst
      )> make_compute_output;
  std::function<void (
      FunctionEnvironment &env, const std::vector<llvm::Value *> &args, const Instance &inst
      )> make_update_state;
  std::function<void (ModuleEnvironment &env)> make_def;

  Primitive(bool is_stateful_,
            unsigned int num_state_bytes_,
            std::function<std::vector<llvm::Value *> (
              FunctionEnvironment &, const std::vector<llvm::Value *> &, const Instance &
              )> make_compute_output_,
            std::function<void(
              FunctionEnvironment &, const std::vector<llvm::Value *> &, const Instance &
              )> make_update_state_,
            std::function<void(ModuleEnvironment &)> make_def_)
    : is_stateful(is_stateful_),
      has_definition(true),
      num_state_bytes(num_state_bytes_),
      make_compute_output(make_compute_output_),
      make_update_state(make_update_state_),
      make_def(make_def_)
  {
  }
  
  Primitive(bool is_stateful_,
            unsigned int num_state_bytes_,
            std::function<std::vector<llvm::Value *> (
              FunctionEnvironment &, const std::vector<llvm::Value *> &, const Instance &
              )> make_compute_output_,
            std::function<void (
              FunctionEnvironment &, const std::vector<llvm::Value *> &, const Instance &
              )> make_update_state_)
    : is_stateful(is_stateful_),
      has_definition(false),
      num_state_bytes(num_state_bytes_),
      make_compute_output(make_compute_output_),
      make_update_state(make_update_state_),
      make_def()
  {
  }

  Primitive(std::function<std::vector<llvm::Value *> (
        FunctionEnvironment &, const std::vector<llvm::Value *> &, const Instance &
        )> make_compute_output_)
    : is_stateful(false),
      has_definition(false),
      num_state_bytes(0),
      make_compute_output(make_compute_output_),
      make_update_state(),
      make_def()
  {
  }

};

}

#endif
