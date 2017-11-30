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

static IFace GenIFace(CoreIR::Module *core_mod)
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

  return IFace(outputs, inputs, true);
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

    Instance inst = defn->makeInstance(name);
    instances.emplace_back(move(inst));
    core_instances.push_back(coreinst);
  }

  for (unsigned i = 0; i < instances.size(); i++) {
    instance_map[core_instances[i]] = &instances[i];
  }

  return make_pair(move(instances), move(instance_map));
}

static void ProcessPrimitive(CoreIR::Module *core_mod,
                             unordered_map<CoreIR::Module *, const Definition *> &mod_map,
                             list<Definition> &definitions)
{
  IFace interface = GenIFace(core_mod);

  Definition defn(core_mod->getName(), move(interface), move(vector<Instance>()));
  definitions.emplace_back(move(defn));
  mod_map[core_mod] = &definitions.back();
}

static ValueSlice CreateSlice(CoreIR::Wireable *source_w, IFace &defn_interface,
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

  const Value* val = nullptr;
  if (parent_w->getKind() == CoreIR::Wireable::WK_Instance) {
    CoreIR::Instance *coreparentinst = static_cast<CoreIR::Instance *>(parent_w);
    Instance *sourceinst = inst_map.find(coreparentinst)->second;
    val = sourceinst->getIFace().get_output(source->getSelStr());
  } else if (parent_w->getKind() == CoreIR::Wireable::WK_Interface) {
    val = defn_interface.get_output(source->getSelStr());
  } else {
    assert(false);
  }
  return ValueSlice(val, parent_idx, is_arrslice ? 1 : val->getWidth());
}

static void SetupIFaceConnections(CoreIR::Wireable *core_w, IFace &iface, IFace &defn_interface,
                                  const unordered_map<CoreIR::Instance *, Instance *> &inst_map)
{
  auto &input_names = iface.get_input_names();

  for (auto &iname : input_names) {
    Input *new_input = iface.get_input(iname);
    CoreIR::Select *in_sel = core_w->sel(iname);
    auto connected = in_sel->getConnectedWireables();

    if (connected.size() == 0) {
      if (in_sel->getType()->getKind() != CoreIR::Type::TK_Array) {
        assert(false);
      }
      vector<ValueSlice> slices;
      for (int i = 0; i < new_input->getWidth(); i++) {
        CoreIR::Select *arr_sel = in_sel->sel(to_string(i));
        connected = arr_sel->getConnectedWireables();
        assert(connected.size() == 1);
        slices.emplace_back(CreateSlice(*connected.begin(), defn_interface, inst_map));
      }
      new_input->connect(Select(move(slices)));
    } else {
      assert(connected.size() == 1);
      ValueSlice slice = CreateSlice(*connected.begin(), defn_interface, inst_map);
      new_input->connect(Select(move(slice)));
    }
  }
}

static void SetupModuleConnections(CoreIR::ModuleDef *core_def, IFace &defn_interface,
                                   const unordered_map<CoreIR::Instance *, Instance *> &inst_map)
{

  for (auto inst_p : core_def->getInstances()) {
    CoreIR::Instance *coreinst = inst_p.second;
    Instance *jitinst = inst_map.find(coreinst)->second;
    IFace &iface = jitinst->getIFace();
    SetupIFaceConnections(coreinst, iface, defn_interface, inst_map);
  }

  SetupIFaceConnections(core_def->getInterface(), defn_interface, defn_interface, inst_map);
}

static void ProcessModules(CoreIR::Module *core_mod,
                           unordered_map<CoreIR::Module *, const Definition *> &mod_map,
                           list<Definition> &definitions)
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

  IFace defn_interface = GenIFace(core_mod);
  vector<Instance> defn_instances;
  unordered_map<CoreIR::Instance *, Instance *> defn_instmap;
  tie(defn_instances, defn_instmap) = GenInstances(core_def, mod_map);

  SetupModuleConnections(core_def, defn_interface, defn_instmap);

  Definition defn(core_mod->getName(), move(defn_interface), move(defn_instances));
  definitions.emplace_back(move(defn));
  mod_map[core_mod] = &definitions.back();

  cout << core_def->getName() << endl;
  for (auto inst_p : core_def->getInstances()) {
    cout << "\t" << inst_p.first << ": " << inst_p.second->getModuleRef()->getNamespace()->getName() << "." << inst_p.second->getModuleRef()->getName() << endl;
  }
}

Circuit BuildFromCoreIR(CoreIR::Module *core_mod)
{
  list<Definition> definitions;
  unordered_map<CoreIR::Module *, const Definition *> mod_map;
  ProcessModules(core_mod, mod_map, definitions);

  Circuit circuit;

  for (auto &defn : definitions) { 
    circuit.AddDefinition(move(defn));
  }

  return circuit;
}

}
