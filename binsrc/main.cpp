#include <cstdlib>
#include <iostream>

#include <jitsim/jitsim.hpp>
#include <jitsim/JIT.hpp>
#include <jitsim/builder.hpp>
#include <jitsim/circuit.hpp>
#include <jitsim/circuit_llvm.hpp>
#include <jitsim/coreir.hpp>
#include <coreir/ir/context.h>
#include <jitsim/builder_hardcoded.hpp>

using namespace std;

JITSim::Circuit loadJSON(const string &str)
{
  using namespace CoreIR;

  Context *ctx = newContext();

  Module* top;
  if (!loadFromFile(ctx, str, &top)) {
    ctx->die();
  }
  if (!top) {
    ctx->die();
  }

  ctx->runPasses({"rungenerators", "flattentypes"});

  JITSim::Circuit circuit = JITSim::BuildFromCoreIR(top);

  deleteContext(ctx);

  return circuit;
}

int main(int argc, char *argv[])
{
  using namespace JITSim;

  if (argc < 2) {
    cerr << "Provide a json file to load\n";
    return 1;
  }

  Circuit circuit = loadJSON(argv[1]);
  circuit.print();

  Builder builder;

  vector<ModuleEnvironment> modules = ModulesForCircuit(builder, circuit);
  for (const ModuleEnvironment & mod : modules) {
    cout << "\n=================\n\n";
    cout << mod.getIRStr();
  }

  initializeNativeTarget();
  JIT jit;

  /*BuilderHardcoded hc_builder;
  JITSim::JIT::ModuleHandle sl_handle = jit.addModule(hc_builder.makeStoreLoadModule());
  std::function<void(char *)> Store = (void(*)(char *))jit.getSymbolAddress("storeConstant");
  void *mem_addr = malloc(1);
  Store((char *)mem_addr);
  std::function<char(char *)> Load = (char(*)(char *))jit.getSymbolAddress("loadConstant");
  char Result = Load((char *)mem_addr);
  printf("%d\n", Result);

  free(mem_addr);
  jit.removeModule(sl_handle);*/

  //JITSim::JIT::ModuleHandle handle = jit.addModule(builder.makeExternModule());
  //std::function<int()> Adder = (int(*)())jit.getSymbolAddress("wrapadd");
  //int Result = Adder();
  //cout << Result << endl;
  //
  //JITSim::JIT::ModuleHandle new_handle = jit.addModule(builder.makeStructModule());

  //jit.removeModule(handle);
  //jit.removeModule(new_handle);
  return 0;
}
