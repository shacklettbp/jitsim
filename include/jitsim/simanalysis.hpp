#ifndef JITSIM_SIMANALYSIS_HPP_INCLUDED
#define JITSIM_SIMANALYSIS_HPP_INCLUDED

#include <jitsim/primitive.hpp>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace JITSim {

class Instance;
class IFace;

class SimInfo
{
private:
  std::vector<const Instance *> stateful_insts;
  std::vector<const Instance *> state_deps;
  std::vector<const Instance *> output_deps;
  std::vector<const Instance *> all_insts;
  std::unordered_map<const Instance *, unsigned> offset_map;
  std::optional<Primitive> primitive;

  bool is_stateful;
  unsigned int num_state_bytes;

  std::vector<const Source *> state_dep_srcs; /* These input sources are directly necessary to update the state */
  std::vector<const Source *> output_dep_srcs; /* These input sources are directly necessary to compute the output */

  void calculateStateOffsets();
  void analyzeStateDeps(const IFace &);
  void analyzeOutputDeps(const IFace &);
  void sortAllInstances();

public:
  SimInfo(const IFace &defn_iface, const std::vector<Instance> &instances);
  SimInfo(const IFace &defn_iface, const Primitive &primitive);

  std::vector<uint8_t> allocateState() const;

  bool isStateful() const { return is_stateful; }
  bool isPrimitive() const { return primitive.has_value(); }

  const std::vector<const Instance *> & getStateDeps() const { return state_deps; }
  const std::vector<const Instance *> & getOutputDeps() const { return output_deps; }
  const std::vector<const Instance *> & getStatefulInstances() const { return stateful_insts; }
  const std::vector<const Instance *> & getAllInstances() const { return all_insts; }
  const std::vector<const Source *> & getStateSources() const { return state_dep_srcs; }
  const std::vector<const Source *> & getOutputSources() const { return output_dep_srcs; }

  unsigned getOffset(const Instance *inst) const { return offset_map.find(inst)->second; }

  unsigned int getNumStateBytes() const { return num_state_bytes; }
  const Primitive& getPrimitive() const { return *primitive; }


  void print(const std::string &prefix = "") const;
};

}

#endif
