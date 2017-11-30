#include <jitsim/Builder.hpp>

namespace JITSim {

using namespace llvm;

Builder::Builder()
: TheBuilder(Context)
{ }
  
std::unique_ptr<Module> Builder::makeModule() {
  // Usage:  ConstantInt::get(context, APInt(nbits, val, is_signed);
  Value *LHS = ConstantInt::get(Context, APInt(32, 1, true));  
  Value *RHS = ConstantInt::get(Context, APInt(32, 2, true));  
  // CreateAdd has some additional boolean fields, check significance
  TheBuilder.CreateAdd(LHS, RHS, "addtmp"); 

  return make_unique<Module>("my cool jit", Context);
}
  
} // end namespace JITSim
