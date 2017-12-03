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

llvm::Value * FunctionEnvironment::lookupValue(const Source *lookup) const
{
  return value_lookup.find(lookup)->second;
}

void FunctionEnvironment::addValue(const Source *key, llvm::Value *val)
{
  value_lookup[key] = val;
}

BasicBlock * FunctionEnvironment::addBasicBlock(const std::string &name, bool setEntry)
{
  BasicBlock *bb = BasicBlock::Create(*context, name, func);
  if (setEntry) {
    ir_builder.SetInsertPoint(bb);
  }

  return bb;
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

  return ModuleEnvironment(move(module), &context);
}
  
} // end namespace JITSim
