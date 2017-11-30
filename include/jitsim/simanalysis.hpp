#ifndef JITSIM_SIMANALYSIS_HPP_INCLUDED
#define JITSIM_SIMANALYSIS_HPP_INCLUDED

#include <vector>

namespace JITSim {

class Instance;

class SimInfo
{
private:
  std::vector<const Instance *> pre_instances;
  std::vector<const Instance *> post_instances;

  bool is_stateful;
  bool is_primitive;
public:
  SimInfo(const std::vector<Instance> &instances);
  SimInfo(bool is_stateful);
};

}

#endif
