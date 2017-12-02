#ifndef JITSIM_SIMANALYSIS_HPP_INCLUDED
#define JITSIM_SIMANALYSIS_HPP_INCLUDED

#include <jitsim/primitive.hpp>

#include <vector>
#include <unordered_map>
#include <optional>

namespace JITSim {

class Instance;
class IFace;

class SimInfo
{
private:
  std::vector<const Instance *> state_deps;
  std::vector<const Instance *> output_deps;
  std::vector<const Instance *> stateful_insts;
  std::unordered_map<const Instance *, unsigned> offset_map;
  std::optional<Primitive> primitive;

  bool is_stateful;
  unsigned int num_state_bytes;

public:
  SimInfo(const IFace &interface, const std::vector<Instance> &instances);
  SimInfo(const Primitive &primitive);

  std::vector<uint8_t> allocateState() const;

  bool isStateful() const { return is_stateful; }
  bool isPrimitive() const { return primitive.has_value(); }

  const std::vector<const Instance *> & getStateDeps() const { return state_deps; }
  const std::vector<const Instance *> & getOutputDeps() const { return output_deps; }
  const std::vector<const Instance *> & getStatefulInstances() const { return stateful_insts; }

  unsigned getOffset(const Instance *inst) const { return offset_map.find(inst)->second; }

  unsigned int getNumStateBytes() const { return num_state_bytes; }
  const Primitive& getPrimitive() const { return *primitive; }


  void print(const std::string &prefix = "") const;
};

}

#endif
