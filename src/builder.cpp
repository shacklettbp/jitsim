#include <jitsim/builder.hpp>

#include <iostream>

extern "C" {
int adder(int lhs, int rhs) {
  return lhs + rhs;
}
} // end extern

namespace JITSim {

using namespace llvm;

Builder::Builder()
: ir_builder(context)
{ }

Value * FunctionEnvironment::lookupValue(const std::string &name)
{
  return named_values.find(name)->second;
}

void FunctionEnvironment::addValue(const std::string &name, Value *val)
{
  named_values[name] = val;
}

FunctionEnvironment ModuleEnvironment::makeFunction(const std::string &name, FunctionType *function_type)
{
  Function *fn = Function::Create(function_type, Function::ExternalLinkage, name, module.get());
  named_functions[name] = fn;

  return FunctionEnvironment(fn, this);
}

std::string ModuleEnvironment::getIRStr() const
{
  std::string str;
  raw_string_ostream OS(str);
  OS << *module;
  OS.flush();
  std::cout << str << std::endl;
}

ModuleEnvironment Builder::makeModule(const std::string &name)
{
  std::unique_ptr<Module> module = make_unique<Module>(StringRef(name), context);

  return ModuleEnvironment(move(module), &context);
}
  
} // end namespace JITSim
