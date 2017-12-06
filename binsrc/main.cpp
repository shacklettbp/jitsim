#include <cstdlib>
#include <iostream>

#include <jitsim/JIT.hpp>
#include <jitsim/builder.hpp>
#include <jitsim/circuit.hpp>
#include <jitsim/circuit_llvm.hpp>
#include <jitsim/coreir.hpp>
#include <coreir/ir/context.h>
#include <coreir/libs/commonlib.h>
#include <jitsim/builder_hardcoded.hpp>

using namespace std;

JITSim::Circuit loadJSON(const string &str)
{
  using namespace CoreIR;

  Context *ctx = newContext();
  CoreIRLoadLibrary_commonlib(ctx);

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

  JITFrontend jit(circuit);

  for (unsigned i = 0; i < 30; i++) {
    jit.setInput("I", 3);
    LLVMStruct output = jit.computeOutput();
    jit.updateState();
    auto val = output.getVal("O");
    cout << val << endl;
  }
  cout << "State: ";
  for (const uint8_t & x : jit.getState()) {
    cout << (int)x;
  }
  cout << endl;

  return 0;
}
