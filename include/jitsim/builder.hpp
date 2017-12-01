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

class Value;
class ModuleEnvironment;

class FunctionEnvironment {
private:
  llvm::Function *func;
  ModuleEnvironment *parent;
  llvm::LLVMContext *context;

  std::unordered_map<const JITSim::Value *, llvm::Value *> named_values; 
  llvm::IRBuilder<> ir_builder;

public:
  FunctionEnvironment(llvm::Function *func_, ModuleEnvironment *parent_);

  llvm::Value * lookupValue(const JITSim::Value *) const;
  void addValue(const JITSim::Value *, llvm::Value *val);

  llvm::BasicBlock * addBasicBlock(const std::string &name, bool setEntry = true);

  llvm::Function * getFunction() { return func; }
  ModuleEnvironment & getModule() { return *parent; }
  llvm::LLVMContext & getContext() { return *context; }
  llvm::IRBuilder<> & getIRBuilder() { return ir_builder; }

  void verify() { llvm::verifyFunction(*func); }
};

class ModuleEnvironment {
private:
  std::unique_ptr<llvm::Module> module;
  llvm::LLVMContext *context;

  std::unordered_map<std::string, llvm::Function *> named_functions;
public:
  ModuleEnvironment(std::unique_ptr<llvm::Module> &&module_, llvm::LLVMContext *context_)
    : module(move(module_)), context(context_)
  {}

  llvm::LLVMContext & getContext() { return *context; }

  llvm::Function * getFunctionDecl(const std::string &name);
  llvm::Function * makeFunctionDecl(const std::string &name, llvm::FunctionType *function_type);
  FunctionEnvironment makeFunction(const std::string &name, llvm::FunctionType *function_type);

  std::unique_ptr<llvm::Module> returnModule() { return move(module); }
  std::string getIRStr() const;
};

class Builder {
  private:
    llvm::LLVMContext context;
  public:

    Builder() {}

    ModuleEnvironment makeModule(const std::string &name);
};

} // end namespace JITSim

#endif // JITSIM_JIT_HPP_INCLUDED
