#include <jitsim/circuit.hpp>

#include <unordered_map>
#include <list>

namespace JITSim {

using namespace std;

Select::Select(ValueSlice &&slice_) 
  : slices()
{
  slices.emplace_back(move(slice_));
}

Select::Select(std::vector<ValueSlice> &&slices_)
  : slices(move(slices_))
{}

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
    output_names.emplace_back(name_pair.first);
    outputs.emplace_back(name_pair.second, this);
  }

  for (unsigned i = 0; i < outputs.size(); i++) {
    output_lookup[output_names[i]] = &outputs[i];
  }

  for (auto &name_pair : inputs_) {
    input_names.emplace_back(name_pair.first);
    inputs.emplace_back(name_pair.second);
  }

  for (unsigned i = 0; i < inputs.size(); i++) {
    input_lookup[input_names[i]] = &inputs[i];
  }
}

const Value * IFace::get_output(const std::string &name) const
{
  return output_lookup.find(name)->second;
}

Value * IFace::get_output(const std::string &name)
{
  return output_lookup.find(name)->second;
}

const Input * IFace::get_input(const std::string &name) const
{
  return input_lookup.find(name)->second;
}

Input * IFace::get_input(const std::string &name)
{
  return input_lookup.find(name)->second;
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
    instances(move(insts)), pre_instances(),
    post_instances()
{
  // FIXME populate pre and post
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
  for (const string &name : interface.get_input_names()) {
    const Input *input = interface.get_input(name);
    outputs.emplace_back(make_pair(name, input->getWidth()));
  }

  for (const string &name : interface.get_output_names()) {
    const Value *val = interface.get_output(name);
    inputs.emplace_back(make_pair(name, val->getWidth()));
  }

  IFace inst_interface(outputs, inputs, false);

  Instance inst = Instance(name, move(inst_interface), defnref);

  return inst;
}

void Circuit::AddDefinition(Definition &&defn)
{
  definitions.emplace_back(move(defn));
  top_defn = &definitions.back();
}

}
