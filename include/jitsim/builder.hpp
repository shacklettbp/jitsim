#ifndef JITSIM_BUILDER_HPP_INCLUDED
#define JITSIM_BUILDER_HPP_INCLUDED

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>

#include <iostream>
#include <unordered_map>
#include <memory>

namespace JITSim {

class Source;
class Sink;
class FunctionEnvironment;

class ModuleEnvironment {
private:
  std::shared_ptr<llvm::Module> module;
  llvm::LLVMContext *context;
  std::unique_ptr<llvm::DIBuilder> di_builder;

  std::unordered_map<std::string, llvm::Function *> named_functions;

  std::unordered_map<const Source *, llvm::Value *> src_value_lookup; 
  std::unordered_map<const Sink *, llvm::Value *> sink_value_lookup; 
public:
  ModuleEnvironment(std::unique_ptr<llvm::Module> &&module_, llvm::LLVMContext *context_)
    : module(move(module_)), context(context_), di_builder(std::make_unique<llvm::DIBuilder>(*module))
  {}

  llvm::LLVMContext & getContext() { return *context; }
  llvm::DIBuilder & getDIBuilder() { return *di_builder; }

  llvm::Function * getFunctionDecl(const std::string &name);
  llvm::Function * makeFunctionDecl(const std::string &name, llvm::FunctionType *function_type);
  FunctionEnvironment makeFunction(const std::string &name, llvm::FunctionType *function_type);

  llvm::Value * lookupValue(const Source *) const;
  llvm::Value * lookupValue(const Sink *) const;
  void addValue(const Source *, llvm::Value *val);
  void addValue(const Sink *, llvm::Value *val);

  std::shared_ptr<llvm::Module> getModule() const { return module; }
  std::string getIRStr() const;

  bool verify() const;
};


class FunctionEnvironment {
private:
  llvm::Function *func;
  ModuleEnvironment *parent;
  llvm::LLVMContext *context;

  llvm::IRBuilder<> ir_builder;
  llvm::BasicBlock *cur_bb;

public:
  FunctionEnvironment(llvm::Function *func_, ModuleEnvironment *parent_);

  llvm::Value * lookupValue(const Source *src) const { return parent->lookupValue(src); }
  llvm::Value * lookupValue(const Sink *sink) const { return parent->lookupValue(sink); }
  void addValue(const Source *src, llvm::Value *val) { parent->addValue(src, val); }
  void addValue(const Sink *sink, llvm::Value *val) { parent->addValue(sink, val); }

  llvm::BasicBlock * addBasicBlock(const std::string &name, bool setEntry = true);
  llvm::BasicBlock * getCurBasicBlock() { return cur_bb; }
  void setCurBasicBlock(llvm::BasicBlock *bb)
  {
    cur_bb = bb;
    ir_builder.SetInsertPoint(cur_bb);
  }
  void addDebugValue(llvm::Value *val, llvm::DILocalVariable *var_info,
                     llvm::DIExpression *expr, const llvm::DILocation *loc)
  { getDIBuilder().insertDbgValueIntrinsic(val, var_info, expr, loc, cur_bb); }

  llvm::Function * getFunction() { return func; }
  ModuleEnvironment & getModule() { return *parent; }
  llvm::LLVMContext & getContext() { return *context; }
  llvm::IRBuilder<> & getIRBuilder() { return ir_builder; }
  llvm::DIBuilder & getDIBuilder();

  bool verify() const;
};

class Builder {
  private:
    llvm::LLVMContext context;
    llvm::DataLayout data_layout;
    std::string triple;
  public:

    Builder(const llvm::DataLayout &dl, const llvm::TargetMachine &target_machine) 
      : data_layout(dl), triple(target_machine.getTargetTriple().getTriple())
    {}

    ModuleEnvironment makeModule(const std::string &name);

    llvm::LLVMContext & getContext() { return context; }
};

} // end namespace JITSim

#endif // JITSIM_JIT_HPP_INCLUDED
