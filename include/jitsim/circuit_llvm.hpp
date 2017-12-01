#ifndef JITSIM_CIRCUIT_LLVM_HPP_INCLUDED
#define JITSIM_CIRCUIT_LLVM_HPP_INCLUDED

#include <jitsim/circuit.hpp>
#include <jitsim/builder.hpp>

#include <llvm/IR/Module.h>

namespace JITSim {

ModuleEnvironment ModuleForDefinition(Builder &builder, const Definition &definition);

std::vector<ModuleEnvironment> ModulesForCircuit(Builder &builder, const Circuit &circuit);

}

#endif
