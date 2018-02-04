#ifndef JITSIM_CIRCUIT_LLVM_HPP_INCLUDED
#define JITSIM_CIRCUIT_LLVM_HPP_INCLUDED

#include <jitsim/circuit.hpp>
#include <jitsim/builder.hpp>

#include <llvm/IR/Module.h>

namespace JITSim {

ModuleEnvironment MakeComputeOutput(Builder &builder, const Definition &definition);
ModuleEnvironment MakeUpdateState(Builder &builder, const Definition &definition);
ModuleEnvironment MakeOutputDeps(Builder &builder, const Definition &definition);
ModuleEnvironment MakeStateDeps(Builder &builder, const Definition &definition);
ModuleEnvironment MakeComputeOutputWrapper(Builder &builder, const Definition &defn);
ModuleEnvironment MakeUpdateStateWrapper(Builder &builder, const Definition &defn);
ModuleEnvironment MakeGetValuesWrapper(Builder &builder, const Definition &defn);

}

#endif
