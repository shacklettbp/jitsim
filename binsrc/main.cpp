#include <cstdlib>
#include <iostream>

#include <jitsim/jitsim.hpp>
#include <jitsim/JIT.hpp>
#include <jitsim/Builder.hpp>
#include <jitsim/circuit.hpp>
#include <jitsim/coreir.hpp>
#include <coreir/ir/context.h>

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
  if (argc < 2) {
    cerr << "Provide a json file to load\n";
    return 1;
  }

  JITSim::Circuit circuit = loadJSON(argv[1]);
  circuit.print();

  JITSim::initializeNativeTarget();
  JITSim::JIT jit;
  JITSim::Builder builder;

  JITSim::JIT::ModuleHandle handle = jit.addModule(builder.makeExternModule());
  std::function<int()> Adder = (int(*)())jit.getSymbolAddress("wrapadd");
  int Result = Adder();
  cout << Result << endl;
  
  JITSim::JIT::ModuleHandle new_handle = jit.addModule(builder.makeStructModule());

  jit.removeModule(handle);
  jit.removeModule(new_handle);
  return 0;
}
