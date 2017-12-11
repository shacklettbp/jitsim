#include <cstdlib>
#include <iostream>
#include <regex>

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
  jit.dumpIR();

  LLVMStruct out = jit.computeOutput();
  cout << "Starting output: ";
  out.dump();
  cout << "\n";

  int advance = 0;
  regex next(R"(next(?:\s+(\d+))?)");
  regex assign(R"(assign\s+(\w+)\s+(\d+))");

  while (true) {
    if (advance == 0) {
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
      } else if (regex_search(input, match, assign)) {
        string in_name = match[1];
        llvm::StringRef strRef = llvm::StringRef(match[2]);
        llvm::APInt val;
        strRef.getAsInteger(10, val);
        jit.setInput(in_name, val);

        out = jit.computeOutput();
        out.dump();
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
