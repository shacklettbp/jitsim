#include <jitsim/builder_hardcoded.hpp>

#include <iostream>

extern "C" {
int hardcoded_adder(int lhs, int rhs) {
  return lhs + rhs;
}
} // end extern

namespace JITSim {

using namespace llvm;

BuilderHardcoded::BuilderHardcoded()
  : ir_builder(context)
{ }
  
std::unique_ptr<Module> BuilderHardcoded::makeStructModule() {
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
  Value *input_pair = fn->args().begin();
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

  // Verify generated code is consistent.
  verifyFunction(*fn);

  return module;
}

std::unique_ptr<Module> BuilderHardcoded::makeExternModule() {

  std::unique_ptr<Module> module = make_unique<Module>("extern", context);
  
  // Arguments are hard-coded.
  Value *lhs = ConstantInt::get(context, APInt(32, 1, true));  
  Value *rhs = ConstantInt::get(context, APInt(32, 2, true));  

  // Create function type: Int(Int, Int).
  std::vector<Type *> int_vec(2, Type::getInt32Ty(context));
  FunctionType *function_type = FunctionType::get(Type::getInt32Ty(context), int_vec, false);

  // Create declaration of external function and wrapper function.
  Function *external_fn = Function::Create(function_type, Function::ExternalLinkage, "hardcoded_adder", module.get());
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

  // Verify generated code is consistent.
  verifyFunction(*fn);

  return module;
}

std::unique_ptr<Module> BuilderHardcoded::makeStoreLoadModule() {
  std::unique_ptr<Module> module = make_unique<Module>("store/load", context);

  /* ----------- BEGIN storeConstant() ------------- */

  // Create function type: void(i8*).
  std::vector<Type *> store_input(1, Type::getInt8PtrTy(context));
  FunctionType *store_fn_type = FunctionType::get(Type::getVoidTy(context), store_input, false);
  // Create declaration of storeConstant().
  Function *store_fn = Function::Create(store_fn_type, Function::ExternalLinkage, "storeConstant", module.get());
  (void)store_fn;
    
  // Create a Basic Block and set the insert point.
  BasicBlock *store_basic_block = BasicBlock::Create(context, "store_entry", store_fn);
  ir_builder.SetInsertPoint(store_basic_block);

  // Get the argument(s).
  Value *store_addr = store_fn->args().begin();
  store_addr->setName("store_addr");
  
  // Store a constant at the input address.
  Value *store_constant = ConstantInt::get(context, APInt(8, 5, true));  
  ir_builder.CreateStore(store_constant, store_addr);

  // Return void.
  ir_builder.CreateRetVoid();

  /* ----------- END storeConstant() ------------- */

  /* ----------- BEGIN loadConstant() ------------- */

  // Create function type: i8(i8*).
  std::vector<Type *> load_input(1, Type::getInt8PtrTy(context));
  FunctionType *load_fn_type = FunctionType::get(Type::getInt8Ty(context), load_input, false);
  // Create declaration
  Function *load_fn = Function::Create(load_fn_type, Function::ExternalLinkage, "loadConstant", module.get());
  (void)load_fn;

  // Create a Basic Block and set the insert point.
  BasicBlock *load_basic_block = BasicBlock::Create(context, "load_entry", load_fn);
  ir_builder.SetInsertPoint(load_basic_block);

  // Get the argument(s).
  Value *load_addr = load_fn->args().begin();
  load_addr->setName("load_addr");
  
  // Load the constant at the input address.
  Value *load_constant = ir_builder.CreateLoad(load_addr, "constant");

  // Return the loaded constant.
  ir_builder.CreateRet(load_constant);

  /* ----------- END loadConstant() ------------- */

  // Debugging: inspect contents of module.
  std::string str;
  raw_string_ostream OS(str);
  OS << *module;
  OS.flush();
  std::cout << str << std::endl;

  // Verify generated code is consistent.
  verifyFunction(*store_fn);
  verifyFunction(*load_fn);

  return module;
}

} // end namespace JITSim
