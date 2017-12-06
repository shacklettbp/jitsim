#ifndef JITSIM_CIRCUIT_LLVM_HPP_INCLUDED
#define JITSIM_CIRCUIT_LLVM_HPP_INCLUDED

#include <jitsim/circuit.hpp>
#include <jitsim/builder.hpp>

#include <llvm/IR/Module.h>

namespace JITSim {

ModuleEnvironment ModuleForDefinition(Builder &builder, const Definition &definition);

std::vector<ModuleEnvironment> ModulesForCircuit(Builder &builder, const Circuit &circuit);

ModuleEnvironment GenerateWrapper(Builder &builder, const Definition &definition);

using WrapperUpdateStateFn = void (*)(const uint8_t *input, uint8_t *state);
using WrapperComputeOutputFn = void (*)(const uint8_t *input, uint8_t *output, uint8_t *state);
}

#endif
