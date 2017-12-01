#ifndef JITSIM_PRIMITIVE_HPP_INCLUDED
#define JITSIM_PRIMITIVE_HPP_INCLUDED

#include <functional>
#include <llvm/IR/Value.h>

namespace JITSim {

struct Primitive {
public:
  bool is_stateful;
  std::function<llvm::Value *()> make_inst;
  std::function<llvm::Value *()> make_def;
  Primitive(bool is_stateful_,
            std::function<llvm::Value *()> make_inst_,
            std::function<llvm::Value *()> make_def_)
    : is_stateful(is_stateful_), make_inst(make_inst_), make_def(make_def_)
  {}
};

}

#endif
