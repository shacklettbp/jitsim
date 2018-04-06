#ifndef JITSIM_SIMANALYSIS_HPP_INCLUDED
#define JITSIM_SIMANALYSIS_HPP_INCLUDED

#include <jitsim/primitive.hpp>
#include <jitsim/optional.hpp>

#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace JITSim {

class Instance;
class IFace;

class SimInfo
{
private:
  std::vector<const Instance *> stateful_insts;
  std::vector<const Instance *> state_deps;
  std::vector<const Instance *> output_deps;
  std::unordered_set<const Instance *> state_deps_lookup;
  std::unordered_set<const Instance *> output_deps_lookup;
  std::unordered_map<const Instance *, unsigned> offset_map;
  std::unordered_map<const Instance *, unsigned> inst_nums;
  optional<Primitive> primitive;

  bool is_stateful;
  unsigned int num_state_bytes;

  std::vector<const Source *> state_dep_srcs; /* These input sources are directly necessary to update the state */
  std::vector<const Source *> output_dep_srcs; /* These input sources are directly necessary to compute the output */

  void calculateStateOffsets();
  void calculateInstanceNumbers(const std::vector<Instance> &instances);
  void analyzeStateDeps(const IFace &);
  void analyzeOutputDeps(const IFace &);

  void initializeState(uint8_t *state) const;
public:
  SimInfo(const IFace &defn_iface, const std::vector<Instance> &instances);
  SimInfo(const IFace &defn_iface, const Primitive &primitive);

  std::vector<uint8_t> allocateState() const;

  bool isStateful() const { return is_stateful; }
  bool isPrimitive() const { return primitive.has_value(); }

  bool isStateDep(const Instance *inst) const { return state_deps_lookup.count(inst); }
  bool isOutputDep(const Instance *inst) const { return output_deps_lookup.count(inst); }
  const std::vector<const Instance *> & getStateDeps() const { return state_deps; }
  const std::vector<const Instance *> & getOutputDeps() const { return output_deps; }
  const std::vector<const Instance *> & getStatefulInstances() const { return stateful_insts; }
  const std::vector<const Source *> & getStateSources() const { return state_dep_srcs; }
  const std::vector<const Source *> & getOutputSources() const { return output_dep_srcs; }

  unsigned getOffset(const Instance *inst) const { return offset_map.find(inst)->second; }
  unsigned getInstNum(const Instance *inst) const { return inst_nums.find(inst)->second; }

  unsigned int getNumStateBytes() const { return num_state_bytes; }
  const Primitive& getPrimitive() const { return *primitive; }


  void print(const std::string &prefix = "") const;
};

}

#endif
