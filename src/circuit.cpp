#include <jitsim/circuit.hpp>

#include <unordered_map>
#include <list>
#include <iostream>
#include <cassert>
#include <string>
#include <sstream>
#include <numeric>

#include <llvm/ADT/StringRef.h>

namespace JITSim {

using namespace std;

ValueSlice::ValueSlice(const Definition *definition_, const Instance *instance_,
                       const Value *val_, int offset_, int width_)
  : definition(definition_), instance(instance_),
    iface(definition ? &definition->getIFace() : &instance->getIFace()),
    val(val_), offset(offset_), width(width_),
    is_whole(offset == 0 && width == val->getWidth()),
    constant()
{}

llvm::APInt makeAPInt(const std::vector<bool> &constant)
{
  string s(constant.size(), '0');
  for (unsigned i = 0; i < constant.size(); i++) {
    if (constant[i]) {
      s[i] = '1';
    }
  }
  return llvm::APInt(constant.size(), llvm::StringRef(s), 2);
}

ValueSlice::ValueSlice(const std::vector<bool> &constant_)
  : definition(nullptr), instance(nullptr), iface(nullptr),
    val(nullptr), offset(0), width(constant_.size()),
    is_whole(true),
    constant(makeAPInt(constant_))
{}

void ValueSlice::extend(const ValueSlice &other)
{
  width += other.width;

  if (isConstant()) {
    llvm::APInt new_const = other.constant->zext(width);
    new_const <<= (width - other.width);
    new_const |= constant->zext(width);
    *constant = new_const;
  }
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

    if ((cur_slice.getValue() == new_slice.getValue() &&
        cur_slice.getOffset() == new_slice.getEndIdx()) ||
        (cur_slice.isConstant() && new_slice.isConstant())) {

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
      direct_value = &slices[0];
    }
  }
}

IFace::IFace(const string &name_,
             vector<Input> &&inputs_,
             vector<Value> &&outputs_,
             bool is_defn)
  : name(name_),
    inputs(move(inputs_)),
    outputs(move(outputs_)), 
    input_lookup(),
    output_lookup(),
    is_definition(is_defn)
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
    interface(name, move(inputs), move(outputs), false),
    defn(defn_)
{
}

const SimInfo & Instance::getSimInfo() const 
{
  return defn->getSimInfo();
}

static vector<Instance> & fully_connect(Definition &defn, vector<Instance> &instances,
                                 function<void (Definition&, vector<Instance> &instances)> make_connections)
{
  make_connections(defn, instances);
  return instances;
}

static string cleanName(const string &name)
{
  string clean = name;
  for (char &c : clean) {
    if (c == '.') {
      c = '_';
    }
  }

  return clean;
}

Definition::Definition(const string &name_,
                       vector<Input> &&inputs,
                       vector<Value> &&outputs,
                       vector<Instance> &&insts,
                       function<void (Definition&, vector<Instance> &instances)> make_connections)
  : name(name_),
    safe_name(cleanName(name)),
    interface("self", move(inputs), move(outputs), true),
    instances(move(insts)),
    siminfo(interface, fully_connect(*this, instances, make_connections))
{}

Definition::Definition(const string &name_,
                       vector<Input> &&inputs,
                       vector<Value> &&outputs,
                       const Primitive &primitive)
  : name(name_),
    interface("self", move(inputs), move(outputs), true),
    instances(),
    siminfo(primitive)
{
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

  if (isConstant()) {
    return constant->toString(10, false);
  } else {
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

  cout << prefix << "  Simulation Information:\n";
  siminfo.print(prefix + "    ");
}

void Circuit::print() const
{
  for (const Definition &defn : definitions) {
    defn.print();
  }
}

}
