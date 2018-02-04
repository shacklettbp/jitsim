#include <jitsim/builder.hpp>

#include <iostream>

extern "C" {
int adder(int lhs, int rhs) {
  return lhs + rhs;
}
} // end extern

namespace JITSim {

using namespace llvm;

FunctionEnvironment::FunctionEnvironment(Function *func_, ModuleEnvironment *parent_)
  : func(func_), parent(parent_), context(&parent->getContext()), ir_builder(*context)
{
}

BasicBlock * FunctionEnvironment::addBasicBlock(const std::string &name, bool setEntry)
{
  BasicBlock *bb = BasicBlock::Create(*context, name, func);
  if (setEntry) {
    ir_builder.SetInsertPoint(bb);
  }

  return bb;
}

DIBuilder & FunctionEnvironment::getDIBuilder()
{
  return parent->getDIBuilder();
}

llvm::Value * ModuleEnvironment::lookupValue(const Source *lookup) const
{
  return src_value_lookup.find(lookup)->second;
}

llvm::Value * ModuleEnvironment::lookupValue(const Sink *lookup) const
{
  return sink_value_lookup.find(lookup)->second;
}

void ModuleEnvironment::addValue(const Source *key, llvm::Value *val)
{
  src_value_lookup[key] = val;
}

void ModuleEnvironment::addValue(const Sink *key, llvm::Value *val)
{
  sink_value_lookup[key] = val;
}

Function * ModuleEnvironment::getFunctionDecl(const std::string &name)
{
  auto iter = named_functions.find(name);
  if (iter == named_functions.end()) {
    return nullptr;
  } else {
    return iter->second;
  }
}

Function * ModuleEnvironment::makeFunctionDecl(const std::string &name, FunctionType *function_type)
{
  Function *fn = Function::Create(function_type, Function::ExternalLinkage, name, module.get());
  named_functions[name] = fn;

  return fn;
}

FunctionEnvironment ModuleEnvironment::makeFunction(const std::string &name, FunctionType *function_type)
{
  Function *fn = makeFunctionDecl(name, function_type);

  return FunctionEnvironment(fn, this);
}

std::string ModuleEnvironment::getIRStr() const
{
  std::string str;
  raw_string_ostream OS(str);
  OS << *module;
  OS.flush();

  return str;
}

ModuleEnvironment Builder::makeModule(const std::string &name)
{
  std::unique_ptr<Module> module = make_unique<Module>(StringRef(name), context);
  module->setDataLayout(data_layout);
  module->setTargetTriple(triple);

  return ModuleEnvironment(move(module), &context);
}

bool FunctionEnvironment::verify() const
{
  bool faulty = verifyFunction(*func, &llvm::errs());
  if (!faulty) {
    return false;
  }

  llvm::errs() << *func;

  return true;
}

bool ModuleEnvironment::verify() const 
{
  bool faulty = verifyModule(*module, &llvm::errs());
  if (!faulty) {
    return false;
  }

  llvm::errs() << *module;

  return true;
}
  
} // end namespace JITSim
