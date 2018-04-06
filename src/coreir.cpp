#include <jitsim/coreir.hpp>
#include <jitsim/circuit.hpp>

#include <coreir/ir/moduledef.h>
#include <coreir/ir/namespace.h>
#include <coreir/ir/types.h>
#include <coreir/ir/value.h>

#include <unordered_map>
#include <list>
#include <utility>
#include <tuple>
#include <string>

#include "coreir_primitives.hpp"

namespace JITSim {

using namespace std;

static bool isConstantModule(CoreIR::Module *mod)
{
  return (mod->getNamespace()->getName() == "corebit" && mod->getName() == "const") ||
         (mod->getNamespace()->getName() == "coreir" && mod->getName() == "const");
}

static bool isTermModule(CoreIR::Module *mod)
{
  return mod->getNamespace()->getName() == "corebit" && mod->getName() == "term";
}

static bool isClock(CoreIR::Type *type)
{
  if (type->getKind() != CoreIR::Type::TK_Named) {
    return false;
  }
  CoreIR::NamedType *named = static_cast<CoreIR::NamedType *>(type);

  return named->toString() == "coreir.clkIn";
}

static IFace GenInterface(CoreIR::Module *core_mod)
{
  vector<Sink> sinks;
  vector<Source> sources;

  vector<ClkSink> clk_sinks;
  vector<ClkSource> clk_sources;

  auto record = core_mod->getType()->getRecord();

  for (auto rpair : record) {
    auto name = rpair.first;
    auto type = rpair.second;
    int width = type->getSize();

    if (type->isInput()) {
      if (isClock(type)) {
        clk_sources.emplace_back(name);
      } else {
        sources.emplace_back(name, width);
      }
    } else {
      if (isClock(type)) {
        clk_sinks.emplace_back(name, nullptr);
      }
      else {
        sinks.emplace_back(name, width);
      }
    }
  }

  return IFace("self", move(sinks), move(sources), move(clk_sinks), move(clk_sources), true);
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

    if (isConstantModule(coreinst_mod) || isTermModule(coreinst_mod)) {
      continue;
    }

    const Definition *defn = mod_map.find(coreinst_mod)->second;

    instances.emplace_back(defn->makeInstance(name));
    Instance &inst = instances.back();
    for (auto &val : coreinst->getModArgs()) {
      if (auto bv = CoreIR::dyn_cast<CoreIR::ConstBitVector>(val.second)) {
        stringstream strm;
        strm << bv->get();
        inst.setArg(val.first, strm.str());
      }
    }
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
  if (isConstantModule(core_mod) || isTermModule(core_mod)) {
    /* Don't process consts as normal modules */
    return;
  }

  auto interface = GenInterface(core_mod);

  Primitive prim = BuildCoreIRPrimitive(core_mod);

  definitions.emplace_back(core_mod->getNamespace()->getName()+"."+core_mod->getName(), move(interface), prim);
  mod_map[core_mod] = &definitions.back();
}

static bool isTermWireable(CoreIR::Wireable *w)
{
  if (w->getKind() != CoreIR::Wireable::WK_Instance) {
    return false;
  }

  CoreIR::Instance *inst = static_cast<CoreIR::Instance *>(w);
  CoreIR::Module *mod = inst->getModuleRef();
  return isTermModule(mod);
}

static bool isConstantWireable(CoreIR::Wireable *w)
{
  if (w->getKind() != CoreIR::Wireable::WK_Instance) {
    return false;
  }

  CoreIR::Instance *inst = static_cast<CoreIR::Instance *>(w);
  CoreIR::Module *mod = inst->getModuleRef();
  return isConstantModule(mod);
}

static SourceSlice CreateSlice(CoreIR::Wireable *source_w, const Definition &defn,
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

  if (isConstantWireable(parent_w)) {
    CoreIR::Instance *const_inst = static_cast<CoreIR::Instance *>(parent_w);
    bool found = false;
    vector<bool> new_const;
    for (auto& arg : const_inst->getModArgs()) {
      if (arg.first == "value") {
        found = true;
        CoreIR::Value* val = arg.second;
        if (const_inst->getModuleRef()->getNamespace()->getName() == "coreir") {
          BitVector bv = val->get<BitVector>();
          for (int i = 0; i < bv.bitLength(); i++) {
            // TODO handle z / x values
            new_const.push_back(bv.get(i).get_char());
          }
        } else {
          bool b = val->get<bool>();
          new_const.push_back(b);
        }
        break;
      }
    }
    (void)found;
    assert(found);

    if (is_arrslice) {
      return SourceSlice(vector<bool>(new_const[parent_idx]));
    } else {
      return SourceSlice(new_const);
    }
  } else {
    const Definition *definition = nullptr;
    const Instance *instance = nullptr;
    const Source* src = nullptr;
    if (parent_w->getKind() == CoreIR::Wireable::WK_Instance) {
      CoreIR::Instance *coreparentinst = static_cast<CoreIR::Instance *>(parent_w);
      instance = inst_map.find(coreparentinst)->second;
      src = instance->getIFace().getSource(source->getSelStr());
    } else if (parent_w->getKind() == CoreIR::Wireable::WK_Interface) {
      src = defn.getIFace().getSource(source->getSelStr());
      definition = &defn;
    } else {
      assert(false);
    }
    assert(src);
    return SourceSlice(definition, instance, src, parent_idx, is_arrslice ? 1 : src->getWidth());
  }
}

static void SetupIFaceConnections(CoreIR::Wireable *core_w, IFace &iface, const Definition &defn,
                                  const unordered_map<CoreIR::Instance *, Instance *> &inst_map)
{
  for (Sink &new_sink : iface.getSinks()) {
    const string &iname = new_sink.getName();

    CoreIR::Select *in_sel = core_w->sel(iname);
    auto connected = in_sel->getConnectedWireables();

    if (connected.size() == 0) {
      if (in_sel->getType()->getKind() != CoreIR::Type::TK_Array) {
        assert(false);
      }
      vector<SourceSlice> slices;
      for (int i = 0; i < new_sink.getWidth(); i++) {
        CoreIR::Select *arr_sel = in_sel->sel(to_string(i));
        connected = arr_sel->getConnectedWireables();
        assert(connected.size() == 1);
        slices.emplace_back(CreateSlice(*connected.begin(), defn, inst_map));
      }
      new_sink.connect(Select(move(slices)));
    } else {
      assert(connected.size() == 1);
      SourceSlice slice = CreateSlice(*connected.begin(), defn, inst_map);
      new_sink.connect(Select(move(slice)));
    }
  }
}

static void SetupModuleConnections(CoreIR::ModuleDef *core_def, Definition &defn,
                                   const unordered_map<CoreIR::Instance *, Instance *> &inst_map)
{

  for (auto inst_p : core_def->getInstances()) {
    CoreIR::Instance *coreinst = inst_p.second;
    if (isConstantWireable(coreinst) || isTermWireable(coreinst)) {
      continue;
    }

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

  auto interface = GenInterface(core_mod);

  definitions.emplace_back(core_mod->getNamespace()->getName()+"."+core_mod->getName(),
                           move(interface), move(defn_instances),
                           [&](Definition &defn, vector<Instance> &instance) {
                             SetupModuleConnections(core_def, defn, defn_instmap);
                           });

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
