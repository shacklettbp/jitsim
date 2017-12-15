#include <cstdlib>
#include <iostream>
#include <regex>

#include <jitsim/jit_frontend.hpp>
#include <jitsim/coreir.hpp>
#include <coreir/ir/context.h>
#include <coreir/libs/commonlib.h>
#include <chrono>

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

  ctx->runPasses({"rungenerators", "flattentypes", "flatten"});

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
  jit.dumpIR();

  std::clock_t start, end;
  start = std::clock();

  unsigned nRuns = 1000000;
  unsigned input = 1;

  for (unsigned i = 0; i < nRuns/2; i++) {
    jit.setInput("in_0", input);
    jit.updateState();

    input = (input + 1) % 25;
  }
  end = std::clock();

  cout << "DONE." << endl;

  double time_ms =
    (end - start) / (double)(CLOCKS_PER_SEC / 1000);

  cout << "Time to compute " << nRuns << " half cycles = " << time_ms << " ms" << endl;
  double time_per_cycle = time_ms / nRuns;
  cout << "Time per cycle for " << nRuns << " = " << time_per_cycle << " ms" << endl;
  
  LLVMStruct out = jit.computeOutput();
  cout << "out = " << out.getValue("out").toString(10, false)  << endl;


  return 0;
}
