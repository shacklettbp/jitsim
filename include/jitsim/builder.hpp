#ifndef JITSIM_BUILDER_HPP_INCLUDED
#define JITSIM_BUILDER_HPP_INCLUDED

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

namespace JITSim {


class ModuleEnvironment;

class FunctionEnvironment {
private:
  llvm::Function *func;
  ModuleEnvironment *parent;
  llvm::LLVMContext *context;

  std::unordered_map<std::string, Value *> named_values; 
  llvm::IRBuilder<> ir_builder;

public:
  FunctionEnvironment(llvm::Function *func_, ModuleEnvironment *parent_)
    : func(func_), parent(parent_), context(parent->getContext())
  {
  }

  Value * lookupValue(const std::string &name);
  void addValue(const std::string &name, Value *val);

  llvm::LLVMContext * getContext() { return context; }
  llvm::IRBuilder<> & getIRBuilder() { return ir_builder; }
};

class ModuleEnvironment {
private:
  std::unique_ptr<llvm::Module> module;
  llvm::LLVMContext *context;

  std::unordered_map<std::string, Function *> named_functions;
public:
  ModuleEnvironment(std::unique_ptr<llvm::Module> &&module_, llvm::LLVMContext *context_)
    : module(move(module_), context(context_))
  {}

  llvm::LLVMContext *getContext { return context; }

  FunctionEnvironment makeFunction(const std::string &name, llvm::FunctionType *function_type);

  std::unique_ptr<llvm::Module> returnModule() { return move(module); }
  std::string getIRStr() const;
};

class Builder {
  private:
    llvm::LLVMContext context;
    //std::map<std::string, llvm::Value *> NamedValues; // Not yet used.
  public:

    Builder();

    ModuleEnvironment makeModule(const std::string &name);

    // Makes hard-coded module
    std::unique_ptr<llvm::Module> makeExternModule();
    std::unique_ptr<llvm::Module> makeStructModule();

    std::unique_ptr<llvm::Module> makeModule(const std::string &name);
    int adder(int lhs, int rhs);
};

} // end namespace JITSim

#endif // JITSIM_JIT_HPP_INCLUDED
