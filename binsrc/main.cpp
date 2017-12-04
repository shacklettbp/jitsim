#include <cstdlib>
#include <iostream>

#include <jitsim/jitsim.hpp>
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

  Builder builder;

  initializeNativeTarget();
  JIT jit;
  vector<JIT::ModuleHandle> handles;

  vector<ModuleEnvironment> modules = ModulesForCircuit(builder, circuit);
  for (const ModuleEnvironment & mod : modules) {
    cout << "\n=================\n\n";
    cout << mod.getIRStr();
  }

  // Add all modules to the JIT.
  for (ModuleEnvironment & mod : modules) {
    handles.push_back(jit.addModule(mod.returnModule()));;
  }
  
  /* ------- Execute here ------- */

  const Definition &top = circuit.getTopDefinition();
  auto update_state = (void (*)(unsigned , uint8_t *))jit.getSymbolAddress(top.getSafeName() + "_update_state");
  auto compute_output = (int (*)(unsigned, uint8_t *))jit.getSymbolAddress(top.getSafeName() + "_compute_output");
  assert(update_state && compute_output);

  vector<uint8_t> state = top.getSimInfo().allocateState();
  for (unsigned i = 0; i < 30; i++) {
    int x = compute_output(i, state.data());
    update_state(i, state.data());
    cout << x << endl;
  }

  cout << "State: ";
  for (unsigned i = 0; i < state.size(); i++) {
    cout << (int)state[i];
  }
  cout << endl;


  /* -------------------------- */
  
  // Remove all modules from the JIT.
  for (JIT::ModuleHandle &handle : handles) {
    jit.removeModule(handle);
  }

  /* --- BuilderHardcoded Testing --- */

  /*BuilderHardcoded hc_builder;
  JIT::ModuleHandle sl_handle = jit.addModule(hc_builder.makeStoreLoadModule());
  std::function<void(char *)> Store = (void(*)(char *))jit.getSymbolAddress("storeConstant");
  void *mem_addr = malloc(1);
  Store((char *)mem_addr);
  std::function<char(char *)> Load = (char(*)(char *))jit.getSymbolAddress("loadConstant");
  char Result = Load((char *)mem_addr);
  printf("%d\n", Result);

  free(mem_addr);
  jit.removeModule(sl_handle);*/

  //JIT::ModuleHandle handle = jit.addModule(builder.makeExternModule());
  //std::function<int()> Adder = (int(*)())jit.getSymbolAddress("wrapadd");
  //int Result = Adder();
  //cout << Result << endl;

  //JIT::ModuleHandle new_handle = jit.addModule(builder.makeStructModule());

  //jit.removeModule(handle);
  //jit.removeModule(new_handle);

  return 0;
}
