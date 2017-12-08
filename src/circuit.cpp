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

SourceSlice::SourceSlice(const Definition *definition_, const Instance *instance_,
                       const Source *val_, int offset_, int width_)
  : definition(definition_), instance(instance_),
    iface(definition ? &definition->getIFace() : &instance->getIFace()),
    val(val_), offset(offset_), width(width_),
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

SourceSlice::SourceSlice(const std::vector<bool> &constant_)
  : definition(nullptr), instance(nullptr), iface(nullptr),
    val(nullptr), offset(0), width(constant_.size()),
    constant(makeAPInt(constant_))
{}

void SourceSlice::extend(const SourceSlice &other)
{
  width += other.width;

  if (isConstant()) {
    llvm::APInt new_const = other.constant->zext(width);
    new_const <<= (width - other.width);
    new_const |= constant->zext(width);
    *constant = new_const;
  }
}

Select::Select(SourceSlice &&slice) 
  : Select(move(vector<SourceSlice>(1, move(slice))))
{}

Select::Select(std::vector<SourceSlice> &&slices_)
  : slices(move(slices_))
{
  compressSlices();
}

void Select::compressSlices()
{
  vector<SourceSlice> new_slices;
  new_slices.emplace_back(slices.front());
  for (unsigned i = 1; i < slices.size(); i++) {
    const SourceSlice &cur_slice = slices[i];
    SourceSlice &new_slice = new_slices.back();

    if ((cur_slice.getSource() == new_slice.getSource() &&
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
    if (slices[0].isConstant() || slices[0].isWhole()) {
      direct_source = &slices[0];
    }
  }
}

IFace::IFace(const string &name_,
             vector<Sink> &&sinks_,
             vector<Source> &&sources_,
             vector<ClkSink> &&clk_sinks_,
             vector<ClkSource> &&clk_sources_,
             bool is_defn)
  : name(name_),
    sinks(move(sinks_)),
    sources(move(sources_)), 
    clk_sinks(move(clk_sinks_)),
    clk_sources(move(clk_sources_)), 
    sink_lookup(),
    source_lookup(),
    is_definition(is_defn)
{
  for (Source &source : sources) {
    source_lookup[source.getName()] = &source;
  }

  for (Sink &sink : sinks) {
    sink_lookup[sink.getName()] = &sink;
  }
}

const Source * IFace::getSource(const string &name) const
{
  return source_lookup.find(name)->second;
}

Source * IFace::getSource(const string &name)
{
  return source_lookup.find(name)->second;
}

const Sink * IFace::getSink(const string &name) const
{
  return sink_lookup.find(name)->second;
}

Sink * IFace::getSink(const string &name)
{
  return sink_lookup.find(name)->second;
}

static vector<Sink> flipSources(const IFace &orig)
{
  vector<Sink> sinks;

  for (const Source &val : orig.getSources()) {
    sinks.emplace_back(val.getName(), val.getWidth());
  }

  return sinks;
}

static vector<Source> flipSinks(const IFace &orig)
{
  vector<Source> sources;

  for (const Sink &sink : orig.getSinks()) {
    sources.emplace_back(sink.getName(), sink.getWidth());
  }

  return sources;
}

static vector<ClkSink> flipClkSources(const IFace &orig)
{
  vector<ClkSink> clk_sinks;

  for (const ClkSource &val : orig.getClkSources()) {
    clk_sinks.emplace_back(val.getName(), nullptr);
  }

  return clk_sinks;
}

static vector<ClkSource> flipClkSinks(const IFace &orig)
{
  vector<ClkSource> clk_sources;

  for (const ClkSink &sink : orig.getClkSinks()) {
    clk_sources.emplace_back(sink.getName());
  }

  return clk_sources;
}

InstanceIFace::InstanceIFace(const string &name_, const IFace &defn_iface)
  : IFace(name_, flipSources(defn_iface), flipSinks(defn_iface), flipClkSources(defn_iface), flipClkSinks(defn_iface), false),
    defn_source_to_sink()
{
  const vector<Source> &sources = defn_iface.getSources();
  const vector<Sink> &sinks = getSinks();
  for (unsigned i = 0; i < sinks.size(); i++) {
    defn_source_to_sink.insert(make_pair(&sources[i], &sinks[i]));
  }
}

Instance::Instance(const string &name_,
                   const Definition *defn_)
  : name(name_),
    interface(name_, defn_->getIFace()),
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
                       IFace &&iface,
                       vector<Instance> &&insts,
                       function<void (Definition&, vector<Instance> &instances)> make_connections)
  : name(name_),
    safe_name(cleanName(name)),
    interface(move(iface)),
    instances(move(insts)),
    instance_lookup(),
    siminfo(interface, fully_connect(*this, instances, make_connections))
{
  for (const Instance &inst : instances) {
    instance_lookup[inst.getName()] = &inst;
  }
}

Definition::Definition(const string &name_,
                       IFace &&iface,
                       const Primitive &primitive)
  : name(name_),
    interface(move(iface)),
    instances(),
    instance_lookup(),
    siminfo(interface, primitive)
{
}

const Instance & Definition::getInstance(const std::string &name) const
{
  return *instance_lookup.find(name)->second;
}

Instance Definition::makeInstance(const string &name) const
{
  return Instance(name, this);
}

string SourceSlice::repr() const
{
  stringstream r;

  if (isConstant()) {
    return constant->toString(10, false);
  } else {
    r << iface->getName() << "." << val->getName();

    if (!isWhole()) {
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
    const SourceSlice &slice = slices[i];
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
  for (const Source &source : sources) {
    cout << prefix << "  " << source.getName() << ": " << source.getWidth() << endl;
  }

  if (is_definition) {
    cout << prefix << "Outputs:\n";
  } else {
    cout << prefix << "Inputs:\n";
  }
  for (const Sink &sink : sinks) {
    cout << prefix << "  " << sink.getName() << ": " << sink.getWidth() << endl;
  }
}

void IFace::print_connectivity(const string &prefix) const 
{
  for (const Sink &sink : sinks) {
    cout << prefix << sink.getName() << ": ";
    assert(sink.isConnected());

    const Select &sel = sink.getSelect();
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

  if (getSimInfo().isPrimitive()) {
    // FIXME print something for primitives
  } else {
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
