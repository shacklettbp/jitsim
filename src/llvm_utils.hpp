#ifndef JITSIM_LLVM_UTILS_HPP_INCLUDED
#define JITSIM_LLVM_UTILS_HPP_INCLUDED

#include "utils.hpp"
#include <llvm/IR/DerivedTypes.h>

namespace JITSim {
  template <typename T>
  static llvm::StructType *ConstructStructType(const std::vector<T> &members, llvm::LLVMContext &context, const std::string &name = "")
  {
    std::vector<llvm::Type *> elem_types;
    for (unsigned i = 0; i < members.size(); i++) {
      const auto &m = condDeref(members[i]);
      elem_types.push_back(llvm::Type::getIntNTy(context, m.getWidth()));
    }
  
    return llvm::StructType::create(context, elem_types, name);
  }
}

#endif
