#ifndef JITSIM_CIRCUIT_LLVM_HPP_INCLUDED
#define JITSIM_CIRCUIT_LLVM_HPP_INCLUDED

#include <jitsim/circuit.hpp>
#include <jitsim/builder.hpp>

#include <llvm/IR/Module.h>

namespace JITSim {

std::unique_ptr<llvm::Module> MakeComputeOutput(Builder &builder, const Definition &definition);
std::unique_ptr<llvm::Module> MakeUpdateState(Builder &builder, const Definition &definition);
std::unique_ptr<llvm::Module> MakeOutputDeps(Builder &builder, const Definition &definition);
std::unique_ptr<llvm::Module> MakeStateDeps(Builder &builder, const Definition &definition);
std::unique_ptr<llvm::Module> MakeComputeOutputWrapper(Builder &builder, const Definition &defn);
std::unique_ptr<llvm::Module> MakeUpdateStateWrapper(Builder &builder, const Definition &defn);
std::unique_ptr<llvm::Module> MakeGetValuesWrapper(Builder &builder, const Definition &defn);

}

#endif
