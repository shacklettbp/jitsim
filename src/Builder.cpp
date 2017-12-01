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
: ir_builder(context)
{ }
  
std::unique_ptr<Module> Builder::makeStructModule() {
  std::unique_ptr<Module> module = make_unique<Module>("structs", context);

  // Create a struct type: (Int, Int)
  std::vector<Type *> elem_types(2, Type::getInt16Ty(context));
  StructType* Int16Pair = StructType::create(context, elem_types, "Int16Pair");

  // Create a function type: Int16Pair(Int16Pair).
  std::vector<Type *> arg_types(1, Int16Pair);
  FunctionType *function_type = FunctionType::get(Int16Pair, arg_types, false);
  
  // Create declaration of function.
  Function *fn = Function::Create(function_type, Function::ExternalLinkage, "square", module.get());

  // Create a Basic Block and set the insert point.
  BasicBlock *basic_block = BasicBlock::Create(context, "entry", fn);
  ir_builder.SetInsertPoint(basic_block);

  // Get the argument(s).
  Value* input_pair = fn->args().begin();
  input_pair->setName("input_pair");
  
  // Access the fields of the struct.
  Value *first_elem = ir_builder.CreateExtractValue(input_pair, { 0 }, "first");
  Value *second_elem = ir_builder.CreateExtractValue(input_pair, { 1 }, "second");

  // Square the struct fields.
  Value *first_elem_sqrd = ir_builder.CreateMul(first_elem, first_elem, "multmp1");
  Value *second_elem_sqrd = ir_builder.CreateMul(second_elem, second_elem, "multmp2"); 

  // Return a new struct with the new fields.
  Value *output_pair = ir_builder.CreateInsertValue(UndefValue::get(Int16Pair), first_elem_sqrd, { 0 }, "output_pair");
  output_pair = ir_builder.CreateInsertValue(output_pair, second_elem_sqrd, { 1 }, "output_pair");
  ir_builder.CreateRet(output_pair);

  // Debugging: inspect contents of module.
  std::string str;
  raw_string_ostream OS(str);
  OS << *module;
  OS.flush();
  std::cout << str << std::endl;

  return module;
}

std::unique_ptr<Module> Builder::makeExternModule() {

  std::unique_ptr<Module> module = make_unique<Module>("my cool jit", context);
  
  // Arguments are hard-coded.
  Value *lhs = ConstantInt::get(context, APInt(32, 1, true));  
  Value *rhs = ConstantInt::get(context, APInt(32, 2, true));  

  // Create function type: Int(Int, Int).
  std::vector<Type *> int_vec(2, Type::getInt32Ty(context));
  FunctionType *function_type = FunctionType::get(Type::getInt32Ty(context), int_vec, false);

  // Create declaration of external function and wrapper function.
  Function *external_fn = Function::Create(function_type, Function::ExternalLinkage, "adder", module.get());
  Function *fn = Function::Create(function_type, Function::ExternalLinkage, "wrapadd", module.get());
  
  // Create a BasicBlock and set the insert point.
  BasicBlock *basic_block = BasicBlock::Create(context, "entry", fn);
  ir_builder.SetInsertPoint(basic_block);

  // Create argument list.
  std::vector<Value *> args_v;
  args_v.push_back(lhs);
  args_v.push_back(rhs);

  // Create the function call and capture the return value.
  Value *return_value = ir_builder.CreateCall(external_fn, args_v, "calltmp");
  ir_builder.CreateRet(return_value);

  // Debugging: inspect contents of module.
  std::string str;
  raw_string_ostream OS(str);
  OS << *module;
  OS.flush();
  std::cout << str << std::endl;

  // Verify generate code is consistent.
  verifyFunction(*fn);

  return module;
}
  
} // end namespace JITSim
