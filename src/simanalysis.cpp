#include <jitsim/simanalysis.hpp>
#include <jitsim/circuit.hpp>

#include <unordered_set>

namespace JITSim {

using namespace std;

static vector<const Instance *> filterStatefulInstances(const vector<Instance> &instances)
{
  vector<const Instance *> stateful;

  for (const Instance &inst : instances) {
    const SimInfo &inst_siminfo = inst.getSimInfo();
    if (inst_siminfo.isStateful()) {
      stateful.push_back(&inst);
    }
  }

  return stateful;
}

/* Returns set of every Source the Sinks in frontier depend on */ unordered_set<const SourceSlice *> getDependencies(unordered_set<const Sink *> frontier)
{
  unordered_set<const SourceSlice *> visited;

  while (frontier.size() > 0) {
    unordered_set<const Sink *> old_frontier = move(frontier);
    frontier.clear();

    for (const Sink *sink : old_frontier) {
      const Select &sel = sink->getSelect();
      for (const SourceSlice &slice : sel.getSlices()) {
        if (slice.isConstant()) {
          continue;
        } else if (slice.isDefinitionAttached()) {
          visited.insert(&slice);
        } else {
          const Instance *depinst = slice.getInstance();
          visited.insert(&slice);

          const SimInfo &inst_info = depinst->getSimInfo();
          for (const Source *src : inst_info.getOutputSources()) {
            const Sink *inst_sink = depinst->getIFace().getSink(src);
            frontier.insert(inst_sink);
          }
        }
      }
    }
  }

  return visited;
}

static unordered_set<const Instance *> getInstances(const unordered_set<const SourceSlice *> &slices)
{
  unordered_set<const Instance *> instances;

  for (const SourceSlice *slice : slices) {
    if (slice->isInstanceAttached()) {
      instances.insert(slice->getInstance());
    }
  }

  return instances;
}

static vector<const Instance *> topoSortInstances(unordered_set<const Instance *> &unsorted)
{
  unordered_set<const Instance *> sorted;
  vector<const Instance *> output;

  while (!unsorted.empty()) {
    bool found = false;
    for (const Instance *candidate : unsorted) {
      const SimInfo &inst_siminfo = candidate->getSimInfo();
      const InstanceIFace &inst_iface = candidate->getIFace();
      bool instance_satisfied = true;
      for (const Source *defn_source : inst_siminfo.getOutputSources()) {
        const Sink *inst_sink = inst_iface.getSink(defn_source);
        bool sink_satisfied = true;

        const Select &sel = inst_sink->getSelect();
        for (const SourceSlice &slice : sel.getSlices()) {
          if (slice.isConstant() || slice.isDefinitionAttached()) {
            continue;
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
    (void)found;
    assert(found);
  }

  return output;
}

static void analyzeDependencies(const IFace &defn_iface,
                                const unordered_set<const Sink *> &frontier,
                                vector<const Instance *> &instances,
                                vector<const Source *> &dep_srcs)
{
  unordered_set<const SourceSlice *> deps = getDependencies(frontier);
  unordered_set<const Instance *> unsorted_insts = getInstances(deps);
  instances = topoSortInstances(unsorted_insts);

  unordered_set<const Source *> dep_src_set;
  for (const SourceSlice *slice : deps) {
    if (slice->isDefinitionAttached()) {
      dep_src_set.insert(slice->getSource());
    }
  }

  /* Preserve ordering of sources */
  for (const Source &src : defn_iface.getSources()) {
    if (dep_src_set.count(&src) > 0) {
      dep_srcs.push_back(&src);
    }
  }
}

void SimInfo::analyzeStateDeps(const IFace &defn_iface)
{
  unordered_set<const Sink *> frontier;
  for (const Instance *stateful_inst : stateful_insts) {
    const SimInfo &inst_info = stateful_inst->getSimInfo();
    for (const Source *src : inst_info.getStateSources()) {
      frontier.insert(stateful_inst->getIFace().getSink(src));
    }
  }

  analyzeDependencies(defn_iface, frontier, state_deps, state_dep_srcs);
}

void SimInfo::analyzeOutputDeps(const IFace &defn_iface)
{
  unordered_set<const Sink *> frontier;
  for (const Sink &sink : defn_iface.getSinks()) {
    frontier.insert(&sink);
  }

  analyzeDependencies(defn_iface, frontier, output_deps, output_dep_srcs);
}

void SimInfo::calculateStateOffsets()
{
  unsigned offset = 0;
  for (const Instance *inst : stateful_insts) {
    offset_map[inst] = offset;
    offset += inst->getDefinition().getSimInfo().getNumStateBytes();
  }

  num_state_bytes = offset;
}

void SimInfo::calculateInstanceNumbers(const vector<Instance> &instances)
{
  unsigned num = 0;
  for (const Instance &inst : instances) {
    inst_nums[&inst] = num;
    num++;
  }
}

SimInfo::SimInfo(const IFace &defn_iface, const vector<Instance> &instances)
  : stateful_insts(filterStatefulInstances(instances)),
    state_deps(),
    output_deps(),
    state_deps_lookup(),
    output_deps_lookup(),
    offset_map(),
    primitive(),
    is_stateful(stateful_insts.size() > 0),
    num_state_bytes(0),
    state_dep_srcs(),
    output_dep_srcs()
{
  if (is_stateful) {
    analyzeStateDeps(defn_iface);
    calculateStateOffsets();
  }
  calculateInstanceNumbers(instances);

  analyzeOutputDeps(defn_iface);

  for (const Instance *inst : output_deps) {
    output_deps_lookup.insert(inst);
  }

  for (const Instance *inst : state_deps) {
    state_deps_lookup.insert(inst);
  }
}

SimInfo::SimInfo(const IFace &defn_iface, const Primitive &primitive_)
  : stateful_insts(),
    state_deps(),
    output_deps(),
    state_deps_lookup(),
    output_deps_lookup(),
    primitive(primitive_),
    is_stateful(primitive->is_stateful),
    num_state_bytes(primitive->num_state_bytes),
    state_dep_srcs(),
    output_dep_srcs()
{
  if (is_stateful) {
    for (const Source &src : defn_iface.getSources()) {
      if (primitive->state_deps.count(src.getName()) > 0) {
        state_dep_srcs.push_back(&src);
      } else if (primitive->output_deps.count(src.getName()) > 0) {
        output_dep_srcs.push_back(&src);
      } else {
        assert(false);
      }
    }
  } else {
    for (const Source &src : defn_iface.getSources()) {
      output_dep_srcs.push_back(&src);
    }
  }
}

void SimInfo::initializeState(uint8_t *state) const
{
  for (const Instance *stateful : stateful_insts) {
    const SimInfo &inst_info = stateful->getSimInfo();
    uint8_t *inst_state = state + getOffset(stateful);
    if (inst_info.isPrimitive() && inst_info.getPrimitive().state_init) {
      inst_info.getPrimitive().state_init(inst_state, *stateful);
    } else {
      inst_info.initializeState(inst_state);
    }
  }
}

vector<uint8_t> SimInfo::allocateState() const
{
  vector<uint8_t> state(num_state_bytes, 0);
  initializeState(state.data());

  return state;
}

void SimInfo::print(const string &prefix) const
{
  cout << prefix << "Bytes for state: " << num_state_bytes << endl;
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

  cout << prefix << "Inputs for compute_outputs:\n";
  for (const Source *src : output_dep_srcs) {
    cout << prefix << "  self." << src->getName() << endl;
  }

  cout << prefix << "Inputs for update_state:\n";
  for (const Source *src : state_dep_srcs) {
    cout << prefix << "  self." << src->getName() << endl;
  }
}

}
