#include <jitsim/simanalysis.hpp>
#include <jitsim/circuit.hpp>

#include <unordered_set>

namespace JITSim {

using namespace std;

unordered_set<const Instance *> get_dependencies(unordered_set<const IFace *> frontier)
{
  unordered_set<const Instance *> visited;

  while (frontier.size() > 0) {
    unordered_set<const IFace *> old_frontier = move(frontier);
    frontier.clear();

    for (const IFace *iface: old_frontier) {
      for (const Input &input : iface->getInputs()) {
        const Select &sel = input.getSelect();
        for (const ValueSlice &slice : sel.getSlices()) {
          if (slice.isConstant() || slice.isDefinitionAttached()) {
            continue;
          }
          const Instance *depinst = slice.getInstance();
          if (visited.find(depinst) == visited.end()) {
            visited.insert(depinst);
            const SimInfo &dep_siminfo = depinst->getSimInfo();
            if (!dep_siminfo.isStateful()) {
              frontier.insert(&depinst->getIFace());
            }
          }
        }
      }
    }
  }

  return visited;
}

void topo_sort_instances(unordered_set<const Instance *> &unsorted, vector<const Instance *> &output)
{
  unordered_set<const Instance *> sorted;

  while (!unsorted.empty()) {
    bool found = false;
    for (const Instance *candidate : unsorted) {
      bool instance_satisfied = true;
      if (!candidate->getSimInfo().isStateful()) {
        for (const Input &input : candidate->getIFace().getInputs()) {
          bool input_satisfied = true;

          const Select &sel = input.getSelect();
          for (const ValueSlice &slice : sel.getSlices()) {
            if (slice.isConstant() || slice.isDefinitionAttached()) {
              continue;
            }
            const Instance *depinst = slice.getInstance();
            if (sorted.count(depinst) == 0) {
              input_satisfied = false;
              break;
            }
          }
          if (!input_satisfied) {
            instance_satisfied = false;
            break;
          }
        }
      }

      if (instance_satisfied) {
        unsorted.erase(candidate);
        sorted.insert(candidate);
        found = true;
        break;
      }
    }
    assert(found);
  }

  for (const Instance *candidate : sorted) {
    output.push_back(candidate);
  }
}

bool order_instances(const IFace &interface,
                     const vector<Instance> &instances,
                     vector<const Instance *> &state_deps,
                     vector<const Instance *> &output_deps,
                     vector<const Instance *> &stateful_insts)
{
  bool stateful = false;
  for (const Instance &inst : instances) {
    const SimInfo &inst_siminfo = inst.getSimInfo();
    if (inst_siminfo.isStateful()) {
      stateful_insts.push_back(&inst);
      stateful = true;
    }
  }

  unordered_set<const IFace *> frontier;
  for (const Instance *stateful_inst : stateful_insts) {
    frontier.insert(&stateful_inst->getIFace());
  }

  unordered_set<const Instance *> visited_state_deps = get_dependencies(frontier);
  topo_sort_instances(visited_state_deps, state_deps);

  frontier.clear();
  frontier.insert(&interface);

  unordered_set<const Instance *> visited_output_deps = get_dependencies(frontier);
  topo_sort_instances(visited_output_deps, output_deps);

  return stateful;
}

SimInfo::SimInfo(const IFace &interface, const vector<Instance> &instances)
  : state_deps(),
    output_deps(),
    stateful_insts(),
    is_stateful(false),
    is_primitive(false)
{
  is_stateful = order_instances(interface, instances, state_deps, output_deps, stateful_insts);
}

SimInfo::SimInfo(const Primitive &primitive)
  : state_deps(),
    output_deps(),
    stateful_insts(),
    is_stateful(primitive.is_stateful),
    is_primitive(true)
{
}

}
