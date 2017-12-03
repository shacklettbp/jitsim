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
      for (const Sink &sink : iface->getSinks()) {
        const Select &sel = sink.getSelect();
        for (const SourceSlice &slice : sel.getSlices()) {
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

vector<const Source *> topo_sort_instances(unordered_set<const Instance *> &unsorted, vector<const Instance *> &output)
{
  unordered_set<const Instance *> sorted;
  unordered_set<const Source *> defn_deps;

  while (!unsorted.empty()) {
    bool found = false;
    for (const Instance *candidate : unsorted) {
      const SimInfo &inst_siminfo = candidate->getDefinition().getSimInfo();
      const InstanceIFace &inst_iface = candidate->getIFace();
      bool instance_satisfied = true;
      for (const Source *defn_source : inst_siminfo.getOutputSources()) {
        const Sink *inst_sink = inst_iface.getSink(defn_source);
        bool sink_satisfied = true;

        const Select &sel = inst_sink->getSelect();
        for (const SourceSlice &slice : sel.getSlices()) {
          if (slice.isConstant()) {
            continue;
          } else if (slice.isDefinitionAttached()) {
            defn_deps.insert(slice.getSource());
          } else {
            const Instance *depinst = slice.getInstance();
            if (sorted.count(depinst) == 0) {
              sink_satisfied = false;
              break;
            }
          }
        }
        if (!sink_satisfied) {
          instance_satisfied = false;
          break;
        }
      }

      if (instance_satisfied) {
        unsorted.erase(candidate);
        sorted.insert(candidate);
        output.push_back(candidate);
        found = true;
        break;
      }
    }
    assert(found);
  }

  vector<const Source *> ret;

  for (const Source *src : defn_deps) {
    ret.push_back(src);
  }

  return ret;
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

  unordered_set<const Instance *> visited_output_deps = get_dependencies({&interface});
  topo_sort_instances(visited_output_deps, output_deps);

  return stateful;
}

SimInfo::SimInfo(const IFace &defn_iface, const vector<Instance> &instances)
  : state_deps(),
    output_deps(),
    stateful_insts(),
    offset_map(),
    primitive(),
    is_stateful(false),
    num_state_bytes(0),
    stateful_sources(),
    output_sources()
{
  is_stateful = order_instances(defn_iface, instances, state_deps, output_deps, stateful_insts);

  unsigned offset = 0;
  for (const Instance *inst : stateful_insts) {
    offset_map[inst] = offset;
    offset += inst->getDefinition().getSimInfo().getNumStateBytes();
  }

  num_state_bytes = offset;
}

SimInfo::SimInfo(const Primitive &primitive_)
  : state_deps(),
    output_deps(),
    stateful_insts(),
    primitive(primitive_),
    is_stateful(primitive->is_stateful),
    num_state_bytes(primitive->num_state_bytes),
    stateful_sources(),
    output_sources()
{
}

vector<uint8_t> SimInfo::allocateState() const
{
  return vector<uint8_t>(num_state_bytes, 0);
}

void SimInfo::print(const string &prefix) const
{
  cout << prefix << "Stateful instances:\n";
  for (const Instance *inst : stateful_insts) {
    cout << prefix << "  " << inst->getName() << endl;
  }

  cout << prefix << "State dependencies:\n";
  for (const Instance *inst : state_deps) {
    cout << prefix << "  " << inst->getName() << endl;
  }

  cout << prefix << "Output dependencies:\n";
  for (const Instance *inst : output_deps) {
    cout << prefix << "  " << inst->getName() << endl;
  }
}

}
