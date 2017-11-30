#include <jitsim/Builder.hpp>

#include <iostream>

extern "C" {
int adder(int lhs, int rhs) {
  return lhs + rhs;
}
} // end extern

namespace JITSim {

using namespace llvm;

Builder::Builder()
: TheBuilder(Context)
{ }
  
std::unique_ptr<Module> Builder::makeModule() {

  std::unique_ptr<Module> module = make_unique<Module>("my cool jit", Context);
  
  // Arguments are hard-coded.
  Value *LHS = ConstantInt::get(Context, APInt(32, 1, true));  
  Value *RHS = ConstantInt::get(Context, APInt(32, 2, true));  

  // Create function type: Int(Int, Int).
  std::vector<Type *> Ints(2, Type::getInt32Ty(Context));
  FunctionType *FT = FunctionType::get(Type::getInt32Ty(Context), Ints, false);

  // Create declaration of external function and wrapper function
  Function *ExternalFn = Function::Create(FT, Function::ExternalLinkage, "adder", module.get());
  Function *Fn = Function::Create(FT, Function::ExternalLinkage, "wrapadd", module.get());
  
  // Create a BasicBlock and set the insert point.
  BasicBlock *BB = BasicBlock::Create(Context, "entry", Fn);
  TheBuilder.SetInsertPoint(BB);

  // Create argument list.
  std::vector<Value *> ArgsV;
  ArgsV.push_back(LHS);
  ArgsV.push_back(RHS);

  // Create the function call and capture the return value.
  Value *RetVal = TheBuilder.CreateCall(ExternalFn, ArgsV, "calltmp");
  TheBuilder.CreateRet(RetVal);

  // Debugging: inspect contents of module.
  std::string Str;
  raw_string_ostream OS(Str);
  OS << *module;
  OS.flush();
  std::cout << Str << std::endl;

  // Verify generate code is consistent.
  verifyFunction(*Fn);

  return module;
}
  
} // end namespace JITSim
