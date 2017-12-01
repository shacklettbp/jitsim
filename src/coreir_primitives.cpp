#include "coreir_primitives.hpp"

#include <coreir/ir/namespace.h>

namespace JITSim {

using namespace std;


Primitive BuildReg(CoreIR::Module *mod)
{
  return Primitive(true, [&]() { return nullptr; }, [&]() { return nullptr; });
}

Primitive BuildAdd(CoreIR::Module *mod)
{
  return Primitive(false, [&]() { return nullptr; }, [&]() { return nullptr; });
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
  static const unordered_map<string,function<Primitive (CoreIR::Module *mod)>> prim_map = InitializeMapping();
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
