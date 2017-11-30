#include <jitsim/circuit.hpp>

#include <unordered_map>
#include <list>
#include <iostream>
#include <cassert>
#include <string>
#include <sstream>

namespace JITSim {

using namespace std;

void ValueSlice::extend(const ValueSlice &other)
{
  width += other.width;
}

Select::Select(ValueSlice &&slice) 
  : Select(move(vector<ValueSlice>(1, move(slice))))
{}

Select::Select(std::vector<ValueSlice> &&slices_)
  : slices(move(slices_))
{
  compressSlices();
}

void Select::compressSlices()
{
  vector<ValueSlice> new_slices;
  new_slices.emplace_back(slices.front());
  for (unsigned i = 1; i < slices.size(); i++) {
    const ValueSlice &cur_slice = slices[i];
    ValueSlice &new_slice = new_slices.back();

    if (cur_slice.getValue() == new_slice.getValue() &&
        cur_slice.getOffset() == new_slice.getEndIdx()) {

        new_slice.extend(cur_slice);
    } else {
      // Can't merge these slices
      new_slices.emplace_back(cur_slice);
    }
  }

  slices = move(new_slices);
  if (slices.size() == 1) {
    has_many_slices = false;
    if (slices[0].isWhole()) {
      direct_value = slices[0].getValue();
    }
  }
}

IFace::IFace(const string &name_,
             vector<Input> &&inputs_,
             vector<Value> &&outputs_,
             bool is_defn,
             const SimInfo *siminfo_)
  : name(name_),
    inputs(move(inputs_)),
    outputs(move(outputs_)), 
    input_lookup(),
    output_lookup(),
    is_definition(is_defn),
    siminfo(siminfo_)
{
  for (Value &output : outputs) {
    output_lookup[output.getName()] = &output;
  }

  for (Input &input : inputs) {
    input_lookup[input.getName()] = &input;
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
                   vector<Input> &&inputs,
                   vector<Value> &&outputs,
                   const Definition *defn_)
  : name(name_),
    interface(name, move(inputs), move(outputs), false, &defn->getSimInfo()),
    defn(defn_)
{
}

vector<Instance> & fully_connect(IFace &iface, vector<Instance> &instances,
                                       function<void (IFace&, vector<Instance> &instances)> make_connections)
{
  make_connections(iface, instances);
  return instances;
}

Definition::Definition(const string &name_,
                       vector<Input> &&inputs,
                       vector<Value> &&outputs,
                       vector<Instance> &&insts,
                       function<void (IFace&, vector<Instance> &instances)> make_connections)
  : name(name_),
    interface("self", move(inputs), move(outputs), true, nullptr),
    instances(move(insts)),
    siminfo(fully_connect(interface, instances, make_connections))
{
  interface.setSimInfo(&siminfo);
}

Definition::Definition(const string &name_,
                       vector<Input> &&inputs,
                       vector<Value> &&outputs)
  : name(name_),
    interface("self", move(inputs), move(outputs), true, nullptr),
    instances(),
    siminfo(false) // FIXME
{
  interface.setSimInfo(&siminfo);
}


Instance Definition::makeInstance(const string &name) const
{
  vector<Value> outputs;
  vector<Input> inputs;
  for (const Input &input : interface.getInputs()) {
    outputs.emplace_back(input.getName(), input.getWidth());
  }

  for (const Value &val  : interface.getOutputs()) {
    inputs.emplace_back(val.getName(), val.getWidth());
  }

  return Instance(name, move(inputs), move(outputs), this);
}

string ValueSlice::repr() const
{
  stringstream r;
  r << iface->getName() << "." << val->getName();

  if (!is_whole) {
    if (width > 1) {
      r << "[" << offset << ":" << getEndIdx() << "]";
    } else {
      r << "[" << offset << "]";
    }
  }

  return r.str();
}

string Select::repr() const
{
  stringstream rep;

  if (has_many_slices) {
    rep << "{";
  }

  for (unsigned i = 0; i < slices.size(); i++) {
    const ValueSlice &slice = slices[i];
    rep << slice.repr();
    if (i != slices.size() - 1) {
      rep << ", ";
    }
  }

  if (has_many_slices) {
    rep << "}";
  }

  return rep.str();
}

void IFace::print(const string &prefix) const 
{
  if (is_definition) {
    cout << prefix << "Inputs:\n";
  } else {
    cout << prefix << "Outputs:\n";
  }
  for (const Value &output : outputs) {
    cout << prefix << "  " << output.getName() << ": " << output.getWidth() << endl;
  }

  if (is_definition) {
    cout << prefix << "Outputs:\n";
  } else {
    cout << prefix << "Inputs:\n";
  }
  for (const Input &input : inputs) {
    cout << prefix << "  " << input.getName() << ": " << input.getWidth() << endl;
  }
}

void IFace::print_connectivity(const string &prefix) const 
{
  for (const Input &input : inputs) {
    cout << prefix << input.getName() << ": ";
    assert(input.isConnected());

    const Select &sel = input.getSelect();
    cout << sel.repr() << endl;
  }
}


void Instance::print(const string &prefix) const
{
  cout << prefix << getName() << ": " << defn->getName() << endl;
}

void Definition::print(const string &prefix) const
{
  cout << prefix << "Module: " << getName() << endl;
  cout << prefix << "  Interface:\n";
  interface.print("    ");

  if (instances.size() == 0) {
    // FIXME print something for primitives
    return;
  }
  cout << prefix << "  Instances:\n";

  for (const Instance &inst : instances) {
    inst.print(prefix + "    ");
  }

  cout << prefix << "  Connectivity:\n";
  for (const Instance &inst : instances) {
    cout << prefix << "    " << inst.getName() << ":\n";
    const IFace &iface = inst.getIFace();
    iface.print_connectivity(prefix + "      ");
  }
  cout << prefix << "    self:\n";
  interface.print_connectivity(prefix + "      ");
}

void Circuit::print() const
{
  for (const Definition &defn : definitions) {
    defn.print();
  }
}

}
