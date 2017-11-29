#include <iostream>

#include <jitsim/jitsim.hpp>
#include <jitsim/JIT.hpp>

using namespace std;

int main(int argc, char *argv[]) {

  JITSim::JIT *jit = new JITSim::JIT();
  (void)jit;
  cout << JITSim::entry() << endl;

  return 0;
}
