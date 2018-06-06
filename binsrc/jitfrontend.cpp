#include <cstdlib>
#include <iostream>
#include <regex>
#include <ctime>

#include <jitsim/jit_frontend.hpp>
#include <jitsim/coreir.hpp>
#include <coreir/ir/context.h>
#include <coreir/libs/commonlib.h>

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

  ctx->addPass(new JITSim::MaterializeArgs);

  ctx->runPasses({"rungenerators", "flattentypes", "materializeargs"});

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

  LLVMStruct out = jit.computeOutput();
  cout << "Starting output: ";
  out.dump();
  cout << "\n";

  int advance = 0;
  regex next(R"(next(?:\s+(\d+))?)");
  regex assign(R"(assign\s+(\w+)\s+(\d+))");
  regex print(R"(print\s+(?:(\w+).)+(\w+))");
  
  int numcycles = -1;
  clock_t start = 0;

  while (true) {
    if (advance == 0) {
      if (numcycles > 0) {
        double secs = ((double)clock() - (double)start) / CLOCKS_PER_SEC;
        //printf("Calculated at %f cycles per second\n", numcycles / secs);
        numcycles = -1;
      }
      string input;
      getline(cin, input);
      if (cin.eof()) {
        break;
      }
      smatch match;
      if (regex_search(input, match, next)) {
        if (match[1] == "") {
          advance = 1;
        } else {
          advance = stoi(match[1]);
        }
        numcycles = advance;
        start = clock();
      } else if (regex_search(input, match, assign)) {
        string in_name = match[1];
        llvm::StringRef strRef = llvm::StringRef(match[2]);
        llvm::APInt val;
        strRef.getAsInteger(10, val);
        jit.setInput(in_name, val);

        out = jit.computeOutput();
        out.dump();
      } else if (regex_search(input, match, print)) {
        vector<string> instances;
        for (unsigned i = 1; i < match.size() - 1; i++) {
          instances.emplace_back(match[i]);
        }
        llvm::APInt val = jit.getValue(instances, match[match.size() - 1]);
        cout << val.toString(10, false) << endl;
      } else {
        cout << "Invalid command\n";
      }

    } else { 
      jit.updateState();
      out = jit.computeOutput();
      out.dump();

      advance--;
    }

  }

  cout << "End State: ";
  for (const uint8_t & x : jit.getState()) {
    cout << (int)x;
  }
  cout << endl;

  return 0;
}
