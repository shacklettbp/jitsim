#ifndef JITSIM_COREIR_PRIMITIVES_HPP_INCLUDED
#define JITSIM_COREIR_PRIMITIVES_HPP_INCLUDED

#include <coreir/ir/module.h>
#include <jitsim/primitive.hpp>

namespace JITSim {
  Primitive BuildCoreIRPrimitive(CoreIR::Module *mod);
}

#endif
