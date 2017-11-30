#include <jitsim/circuit.hpp>

#include <unordered_map>
#include <list>
#include <iostream>

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

const Value * IFace::getOutput(const std::string &name) const
{
  return output_lookup.find(name)->second;
}

Value * IFace::getOutput(const std::string &name)
{
  return output_lookup.find(name)->second;
}

const Input * IFace::getInput(const std::string &name) const
{
  return input_lookup.find(name)->second;
}

Input * IFace::getInput(const std::string &name)
{
  return input_lookup.find(name)->second;
}

Instance::Instance(const string &name_,
                   const vector<pair<string, int>>& outputs,
                   const vector<pair<string, int>>& inputs,
                   const Definition *defn_)
  : name(name_),
    interface(outputs, inputs, false),
    defn(defn_)
{
}

Definition::Definition(const string &name_,
                       const vector<pair<string, int>>& outputs,
                       const vector<pair<string, int>>& inputs,
                       vector<Instance> &&insts)
  : name(name_),
    interface(outputs, inputs, true),
    instances(move(insts)),
    pre_instances(),
    post_instances()
{
  // FIXME populate pre and post
}

Instance Definition::makeInstance(const string &name) const
{
  vector<pair<string, int>> outputs;
  vector<pair<string, int>> inputs;
  for (const string &name : interface.getInputNames()) {
    const Input *input = interface.getInput(name);
    outputs.emplace_back(make_pair(name, input->getWidth()));
  }

  for (const string &name : interface.getOutputNames()) {
    const Value *val = interface.getOutput(name);
    inputs.emplace_back(make_pair(name, val->getWidth()));
  }

  Instance inst = Instance(name, outputs, inputs, this);

  return inst;
}

void IFace::print(const string &prefix) const 
{
  if (is_definition) {
    cout << prefix << "Inputs:\n";
  } else {
    cout << prefix << "Outputs:\n";
  }
  for (const string &oname : output_names) {
    const Value *output = getOutput(oname);
    cout << prefix << "  " << oname << ": " << output->getWidth() << endl;
  }

  if (is_definition) {
    cout << prefix << "Outputs:\n";
  } else {
    cout << prefix << "Inputs:\n";
  }
  for (const string &iname : input_names) {
    const Input *input = getInput(iname);
    cout << prefix << "  " << iname << ": " << input->getWidth() << endl;
  }
}

void Instance::print(const string &prefix) const
{
  cout << prefix << getName() << ": " << defn->getName() << endl;
}

void Definition::print(const string &prefix) const
{
  cout << prefix << "Module: " << getName() << endl;
  cout << "  Interface:\n";
  interface.print("    ");

  if (instances.size() == 0) {
    // FIXME print something for primitives
    return;
  }
  cout << "  Instances:\n";

  for (const Instance &inst : instances) {
    inst.print(prefix + "    ");
  }
}

void Circuit::print() const
{
  for (const Definition &defn : definitions) {
    defn.print();
  }
}

}
