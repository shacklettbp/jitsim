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
      int width = inst.getIFace().getOutputs()[0].getWidth();

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
      llvm::Value *first = args[0];
      llvm::Value *second = args[1];
      llvm::Value *sum = env.getIRBuilder().CreateAdd(first, second, "add");
      
      return std::vector<llvm::Value *> { sum };
    }
  );
}

static unordered_map<string,function<Primitive (CoreIR::Module *mod)>> InitializeMapping()
{
  unordered_map<string,function<Primitive (CoreIR::Module *mod)>> m;
  m["coreir.reg"] = BuildReg;
  m["coreir.add"] = BuildAdd;

  return m;
}

Primitive BuildCoreIRPrimitive(CoreIR::Module *mod)
{
  static const unordered_map<string,function<Primitive (CoreIR::Module *mod)>> prim_map =
    InitializeMapping();
  string fullname = mod->getNamespace()->getName() + "." + mod->getName();

  if (prim_map.count(fullname) == 0) {
    cerr << fullname << endl;
    assert(false);
  }

  auto iter = prim_map.find(fullname);
  assert(iter != prim_map.end());

  return iter->second(mod);
}

}
