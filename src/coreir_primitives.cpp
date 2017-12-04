#include "coreir_primitives.hpp"

#include <coreir/ir/namespace.h>

#include <jitsim/circuit.hpp>

namespace JITSim {

using namespace std;


Primitive BuildReg(CoreIR::Module *mod)
{
  return Primitive(true, 1,
    [&](auto &env, auto &args, auto &inst)
    {
      int width = inst.getIFace().getSources()[0].getWidth();
      llvm::Value *addr = env.getIRBuilder().CreateBitCast(args[0], llvm::Type::getIntNPtrTy(env.getContext(), width));
      llvm::Value *output = env.getIRBuilder().CreateLoad(addr, "output");

      return std::vector<llvm::Value *> { output };
    },
    [&](auto &env, auto &args, auto &inst)
    {
      int width = inst.getIFace().getSources()[0].getWidth();

      llvm::Value *input = args[0];
      llvm::Value *addr = env.getIRBuilder().CreateBitCast(args[1], llvm::Type::getIntNPtrTy(env.getContext(), width));
      env.getIRBuilder().CreateStore(input, addr);
    }
  );
}

Primitive BuildAdd(CoreIR::Module *mod)
{
  return Primitive(
    [&](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *sum = env.getIRBuilder().CreateAdd(lhs, rhs, "sum");
      
      return std::vector<llvm::Value *> { sum };
    }
  );
}

Primitive BuildMul(CoreIR::Module *mod)
{
  return Primitive(
    [&](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *prod = env.getIRBuilder().CreateMul(lhs, rhs, "prod");

      return std::vector<llvm::Value *> { prod };
    }
  );
}      

Primitive BuildEq(CoreIR::Module *mod)
{
  return Primitive(
    [&](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpEQ(lhs, rhs, "eq_comp");

      return std::vector<llvm::Value *> { comp };
    }
  );
}      
      
Primitive BuildNeq(CoreIR::Module *mod)
{
  return Primitive(
    [&](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpNE(lhs, rhs, "neq_comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}      

Primitive BuildMux(CoreIR::Module *mod)
{
  return Primitive(
    [&](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *sel = args[2];
      //llvm::Function *fn = env.getIRBuilder().GetInsertBlock()->getParent();

      llvm::Value *cond = env.getIRBuilder().CreateICmpEQ(sel,
                                      llvm::ConstantInt::get(env.getContext(), llvm::APInt(1, 0)),
                                      "ifcond");

      //llvm::Value *cond = env.getIRBuilder().CreateFCmpONE(..,
      //                                                     ConstantInt::get(context, APInt(0)),
      //                                                     "ifcond");
      llvm::BasicBlock *if_bb = env.addBasicBlock("if_block");
      llvm::BasicBlock *else_bb = env.addBasicBlock("else_block");
      llvm::BasicBlock *cont_bb = env.addBasicBlock("cont_block");


      (void)lhs;
      (void)rhs;
      (void)sel;
      (void)cond;
      (void)if_bb;
      (void)else_bb;
      (void)cont_bb;

      // if sel == [0] then sum = lhs
      // else sum = rhs
      return std::vector<llvm::Value *> ();
    }
  );
}      
      
Primitive BuildMem(CoreIR::Module *mod)
{
  return Primitive(
    [&](auto &env, auto &args, auto &inst)
    {
      return std::vector<llvm::Value *> ();
    }
  );
}      
      
static unordered_map<string,function<Primitive (CoreIR::Module *mod)>> InitializeMapping()
{
  unordered_map<string,function<Primitive (CoreIR::Module *mod)>> m;
  m["coreir.reg"] = BuildReg;
  m["coreir.add"] = BuildAdd;
  m["coreir.mul"] = BuildMul;
  m["coreir.eq"] = BuildEq;
  m["coreir.neq"] = BuildNeq;
  m["coreir.mux"] = BuildMux;
  m["coreir.mem"] = BuildMem;

  return m;
}

Primitive BuildCoreIRPrimitive(CoreIR::Module *mod)
{
  static const unordered_map<string,function<Primitive (CoreIR::Module *mod)>> prim_map =
    InitializeMapping();
  string fullname = mod->getNamespace()->getName() + "." + mod->getName();

  if (prim_map.count(fullname) == 0) {
    cerr << "Unsupported primitive " << fullname << endl;
    assert(false);
  }

  auto iter = prim_map.find(fullname);
  assert(iter != prim_map.end());

  return iter->second(mod);
}

}
