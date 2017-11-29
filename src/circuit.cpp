#include <jitsim/circuit.hpp>

#include <coreir/ir/moduledef.h>

#include <unordered_set>

namespace JITSim {

using namespace std;

IFace::IFace(const vector<pair<string, int>>& outputs_,
             const vector<pair<string, int>>& inputs_,
             bool is_defn)
  : outputs(), 
    inputs(),
    output_names(),
    input_names(),
    input_lookup(),
    output_lookup(),
    is_definition(is_defn)
{
  for (auto &name_pair : outputs_) {
    output_names.push_back(name_pair.first);
    outputs.push_back(Value(name_pair.second, this));
    output_lookup[name_pair.first] = outputs.size() - 1;
  }

  for (auto &name_pair : inputs_) {
    input_names.push_back(name_pair.first);
    inputs.push_back(Input(name_pair.second));
    input_lookup[name_pair.first] = inputs.size() - 1;
  }
}

const Value & IFace::get_output(const std::string &name) const
{
  auto iter = output_lookup.find(name);
  return outputs[iter->second];
}

const Input & IFace::get_input(const std::string &name) const
{
  auto iter = input_lookup.find(name);
  return inputs[iter->second];
}

Instance::Instance(const string &name_, IFace &&iface, const DefnRef *defn_) 
  : name(name_),
    interface(move(iface)),
    defn(defn_)
{
}

Definition::Definition(const string &name_, IFace &&iface, vector<Instance> &&insts)
  : name(name_),
    interface(move(iface)),
    instances(move(insts)),
    pre_instances(),
    post_instances()
{
}

const DefnRef *Definition::getRef() const
{
  return nullptr;
}

Instance Definition::makeInstance(const string &name) const
{
  const DefnRef *defnref = getRef();

  vector<pair<string, int>> outputs;
  vector<pair<string, int>> inputs;
  for (const string &name : interface.get_output_names()) {
    const Input &input = interface.get_input(name);
    outputs.emplace_back(make_pair(name, input.getWidth()));
  }

  for (const string &name : interface.get_input_names()) {
    const Value &val = interface.get_output(name);
    inputs.emplace_back(make_pair(name, val.getWidth()));
  }

  IFace inst_interface(outputs, inputs, false);

  Instance inst = Instance(name, move(inst_interface), defnref);

  return inst;
}

static void ProcessCoreIRModules(CoreIR::ModuleDef *core_def, unordered_set<CoreIR::ModuleDef *> &visited_modules,
                                 vector<Definition> &definitions)
{
  if (visited_modules.find(core_def) == visited_modules.end()) {
    visited_modules.insert(core_def);
  } else {
    return;
  }

  auto instances = core_def->getInstances();

  for (auto inst_p : instances) {
    auto inst = inst_p.second;
    auto mod = inst->getModuleRef();

    if (mod->hasDef()) {
      ProcessCoreIRModules(mod->getDef(), visited_modules, definitions);
    }
  }

  cout << core_def->getName() << endl;
  for (auto inst_p : instances) {
    cout << "\t" << inst_p.first << ": " << inst_p.second->getModuleRef()->getLongName() << endl;
  }
}

Circuit BuildFromCoreIR(CoreIR::Module *core_mod)
{
  vector<Definition> definitions;
  unordered_set<CoreIR::ModuleDef *> visited;
  ProcessCoreIRModules(core_mod->getDef(), visited, definitions);

  return Circuit();
}

}
