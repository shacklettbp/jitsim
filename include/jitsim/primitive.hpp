#ifndef JITSIM_PRIMITIVE_HPP_INCLUDED
#define JITSIM_PRIMITIVE_HPP_INCLUDED

#include <functional>
#include <jitsim/builder.hpp>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>

namespace JITSim {

struct Primitive {
private:
  bool has_definition;

public:
  bool is_stateful;
  std::function<llvm::Value *(FunctionEnvironment &env, const std::vector<llvm::Value *> &)> make_compute_output;
  std::function<void (FunctionEnvironment &env, const std::vector<llvm::Value *> &)> make_update_state;
  std::function<void (ModuleEnvironment &env)> make_def;

  Primitive(bool is_stateful_,
            std::function<llvm::Value *(FunctionEnvironment &, const std::vector<llvm::Value *> &)> make_compute_output_,
            std::function<void(FunctionEnvironment &, const std::vector<llvm::Value *> &)> make_update_state_,
            std::function<void(ModuleEnvironment &)> make_def_)
    : is_stateful(is_stateful_),
      make_compute_output(make_compute_output_),
      make_update_state(make_update_state_),
      make_def(make_def_)
  {
    has_definition = true;
  }
  
  Primitive(bool is_stateful_,
            std::function<llvm::Value *(FunctionEnvironment &, const std::vector<llvm::Value *> &)> make_compute_output_,
            std::function<void(FunctionEnvironment &, const std::vector<llvm::Value *> &)> make_update_state_)
    : is_stateful(is_stateful_),
      make_compute_output(make_compute_output_),
      make_update_state(make_update_state_),
      make_def()
  {
    has_definition = false;
  }

};

}

#endif
