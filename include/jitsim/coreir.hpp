#ifndef JITSIM_COREIR_HPP_INCLUDED
#define JITSIM_COREIR_HPP_INCLUDED

#include <coreir/ir/module.h>
#include <coreir/ir/passes.h>
#include <jitsim/circuit.hpp>
#include <unordered_map>

namespace JITSim {

Circuit BuildFromCoreIR(CoreIR::Module *core_mod);

class MaterializeArgs : public CoreIR::ContextPass {
  private:
    bool materializeArgs(CoreIR::Instance *instance);
    bool traverseInstance(CoreIR::Instance *);
    std::unordered_map<std::string, CoreIR::Module *> uniquified_modules;
  public:
    static std::string ID;
    MaterializeArgs() :
      ContextPass(ID, "Gets rid of modargs on non primitives") {}
    bool runOnContext(CoreIR::Context *);
};
}

#endif
