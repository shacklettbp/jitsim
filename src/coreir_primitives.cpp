#include "coreir_primitives.hpp"

#include <coreir/ir/namespace.h>

namespace JITSim {

using namespace std;


Primitive BuildReg(CoreIR::Module *mod)
{
  return Primitive(true, 1,
    [&](auto &env, auto &args, auto &inst)
    {
      //llvm::Value *addr = args[0];
      //llvm::Value *output = env.getIRBuilder().CreateLoad(addr, "output");
      llvm::Value *output = llvm::ConstantInt::get(env.getContext(), llvm::APInt(1, 0, false));

      return std::vector<llvm::Value *> { output };
    },
    [&](auto &env, auto &args, auto &inst)
    {
      llvm::Value *input = args[0];
      llvm::Value *addr = args[1];
      env.getIRBuilder().CreateStore(input, addr);
    },
    [&](auto &env)
    {
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
