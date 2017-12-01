#ifndef JITSIM_SIMANALYSIS_HPP_INCLUDED
#define JITSIM_SIMANALYSIS_HPP_INCLUDED

#include <jitsim/primitive.hpp>

#include <vector>

namespace JITSim {

class Instance;
class IFace;

class SimInfo
{
private:
  std::vector<const Instance *> state_deps;
  std::vector<const Instance *> output_deps;
  std::vector<const Instance *> stateful_insts;

  bool is_stateful;
  bool is_primitive;
public:
  SimInfo(const IFace &interface, const std::vector<Instance> &instances);
  SimInfo(const Primitive &primitive);

  bool isStateful() const { return is_stateful; }
  bool isPrimitive() const { return is_primitive; }

  const std::vector<const Instance *> & getStateDeps() const { return state_deps; }
  const std::vector<const Instance *> & getOutputDeps() const { return output_deps; }
  const std::vector<const Instance *> & getStatefulInstances() const { return stateful_insts; }


  void print(const std::string &prefix = "") const;
};

}

#endif
