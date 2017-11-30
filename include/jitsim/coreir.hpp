#ifndef JITSIM_COREIR_HPP_INCLUDED
#define JITSIM_COREIR_HPP_INCLUDED

#include <coreir/ir/module.h>
#include <jitsim/circuit.hpp>

namespace JITSim {

Circuit BuildFromCoreIR(CoreIR::Module *core_mod);

}

#endif
