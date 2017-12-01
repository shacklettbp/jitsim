#ifndef JITSIM_CIRCUIT_LLVM_HPP_INCLUDED
#define JITSIM_CIRCUIT_LLVM_HPP_INCLUDED

#include <jitsim/circuit.hpp>
#include <jitsim/builder.hpp>

#include <llvm/IR/Module.h>

namespace JITSim {

std::unique_ptr<llvm::Module> ModuleForDefinition(Builder &builder, const Definition &definition);

}

#endif
