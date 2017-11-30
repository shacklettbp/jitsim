#include <jitsim/coreir.hpp>
#include <jitsim/circuit.hpp>

#include <coreir/ir/moduledef.h>
#include <coreir/ir/namespace.h>
#include <coreir/ir/types.h>

#include <unordered_map>
#include <list>
#include <utility>
#include <tuple>
#include <string>

namespace JITSim {

using namespace std;

static pair<vector<pair<string, int>>, vector<pair<string, int>>>
GenInterface(CoreIR::Module *core_mod)
{
  vector<pair<string, int>> outputs, inputs;

  auto record = core_mod->getType()->getRecord();

  for (auto rpair : record) {
    auto name = rpair.first;
    auto type = rpair.second;
    int width = type->getSize();

    if (type->isInput()) {
      outputs.emplace_back(make_pair(name, width));
    } else {
      inputs.emplace_back(make_pair(name, width));
    }
  }

  return make_pair(outputs, inputs);
}

static pair<vector<Instance>, unordered_map<CoreIR::Instance *, Instance *>>
GenInstances(CoreIR::ModuleDef *core_def,
             const unordered_map<CoreIR::Module *, const Definition *> &mod_map)
{
  vector<Instance> instances;
  vector<CoreIR::Instance *> core_instances;
  unordered_map<CoreIR::Instance *, Instance *> instance_map;

  for (auto coreinst_p : core_def->getInstances()) {
    auto name = coreinst_p.first;

    auto coreinst = coreinst_p.second;
    auto coreinst_mod = coreinst->getModuleRef();

    const Definition *defn = mod_map.find(coreinst_mod)->second;

    instances.emplace_back(move(defn->makeInstance(name)));
    core_instances.push_back(coreinst);
  }

  for (unsigned i = 0; i < instances.size(); i++) {
    instance_map[core_instances[i]] = &instances[i];
  }

  return make_pair(move(instances), move(instance_map));
}

static void ProcessPrimitive(CoreIR::Module *core_mod,
                             unordered_map<CoreIR::Module *, const Definition *> &mod_map,
                             deque<Definition> &definitions)
{
  auto interface = GenInterface(core_mod);

  definitions.emplace_back(core_mod->getName(), interface.first, interface.second, move(vector<Instance>()));
  mod_map[core_mod] = &definitions.back();
}

static ValueSlice CreateSlice(CoreIR::Wireable *source_w, const Definition &defn,
                              const unordered_map<CoreIR::Instance *, Instance *> &inst_map)
{
  assert(source_w->getKind() == CoreIR::Wireable::WK_Select);

  CoreIR::Select *source = static_cast<CoreIR::Select *>(source_w);
  CoreIR::Wireable *parent_w = source->getParent();

  int parent_idx = 0;
  bool is_arrslice = false;
  if (parent_w->getType()->getKind() == CoreIR::Type::TK_Array) {
    parent_idx = stoul(source->getSelStr());
    source = static_cast<CoreIR::Select *>(parent_w);
    parent_w = source->getParent();
    is_arrslice = true;
  } 

  const IFace* iface = nullptr;
  const Value* val = nullptr;
  if (parent_w->getKind() == CoreIR::Wireable::WK_Instance) {
    CoreIR::Instance *coreparentinst = static_cast<CoreIR::Instance *>(parent_w);
    Instance *sourceinst = inst_map.find(coreparentinst)->second;
    iface = &sourceinst->getIFace();
    val = iface->getOutput(source->getSelStr());
  } else if (parent_w->getKind() == CoreIR::Wireable::WK_Interface) {
    iface = &defn.getIFace();
    val = iface->getOutput(source->getSelStr());
  } else {
    assert(false);
  }
  return ValueSlice(iface, val, parent_idx, is_arrslice ? 1 : val->getWidth());
}

static void SetupIFaceConnections(CoreIR::Wireable *core_w, IFace &iface, const Definition &defn,
                                  const unordered_map<CoreIR::Instance *, Instance *> &inst_map)
{
  for (Input &new_input : iface.getInputs()) {
    const string &iname = new_input.getName();

    CoreIR::Select *in_sel = core_w->sel(iname);
    auto connected = in_sel->getConnectedWireables();

    if (connected.size() == 0) {
      if (in_sel->getType()->getKind() != CoreIR::Type::TK_Array) {
        assert(false);
      }
      vector<ValueSlice> slices;
      for (int i = 0; i < new_input.getWidth(); i++) {
        CoreIR::Select *arr_sel = in_sel->sel(to_string(i));
        connected = arr_sel->getConnectedWireables();
        assert(connected.size() == 1);
        slices.emplace_back(CreateSlice(*connected.begin(), defn, inst_map));
      }
      new_input.connect(Select(move(slices)));
    } else {
      assert(connected.size() == 1);
      ValueSlice slice = CreateSlice(*connected.begin(), defn, inst_map);
      new_input.connect(Select(move(slice)));
    }
  }
}

static void SetupModuleConnections(CoreIR::ModuleDef *core_def, Definition &defn,
                                   const unordered_map<CoreIR::Instance *, Instance *> &inst_map)
{

  for (auto inst_p : core_def->getInstances()) {
    CoreIR::Instance *coreinst = inst_p.second;
    Instance *jitinst = inst_map.find(coreinst)->second;
    IFace &iface = jitinst->getIFace();
    SetupIFaceConnections(coreinst, iface, defn, inst_map);
  }

  SetupIFaceConnections(core_def->getInterface(), defn.getIFace(), defn, inst_map);
}

static void ProcessModules(CoreIR::Module *core_mod,
                           unordered_map<CoreIR::Module *, const Definition *> &mod_map,
                           deque<Definition> &definitions)
{
  if (mod_map.find(core_mod) != mod_map.end()) {
    return;
  }

  if (!core_mod->hasDef()) {
    ProcessPrimitive(core_mod, mod_map, definitions);
    return;
  }

  auto core_def = core_mod->getDef(); 

  for (auto inst_p : core_def->getInstances()) {
    auto inst = inst_p.second;
    auto instmod = inst->getModuleRef();

    ProcessModules(instmod, mod_map, definitions);
  }

  vector<Instance> defn_instances;
  unordered_map<CoreIR::Instance *, Instance *> defn_instmap;
  tie(defn_instances, defn_instmap) = GenInstances(core_def, mod_map);
  auto defn_if = GenInterface(core_mod);


  definitions.emplace_back(core_mod->getName(), defn_if.first, defn_if.second, move(defn_instances));
  SetupModuleConnections(core_def, definitions.back(), defn_instmap);

  mod_map[core_mod] = &definitions.back();
}

Circuit BuildFromCoreIR(CoreIR::Module *core_mod)
{
  deque<Definition> definitions;
  unordered_map<CoreIR::Module *, const Definition *> mod_map;
  ProcessModules(core_mod, mod_map, definitions);

  return Circuit(move(definitions));
}

}
