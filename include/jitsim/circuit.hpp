#ifndef JITSIM_CIRCUIT_HPP_INCLUDED
#define JITSIM_CIRCUIT_HPP_INCLUDED

#include <unordered_map>
#include <string>
#include <vector>
#include <list>
#include <utility>
#include <optional>

namespace JITSim {

class IFace;
class ValueSlice;

class Value {
private:
  int width;

  const IFace *attached_interface;
public:
  Value(int w, const IFace *attached_to)
    : width(w), attached_interface(attached_to)
  {}

  ValueSlice getSlice(int offset, int width) const;

  const IFace *getInterface() const { return attached_interface; }

  int getWidth() const { return width; }
};

class ValueSlice {
private:
  const Value *val;
  int offset;
  int width;
public:
  ValueSlice(const Value *val_, int offset_, int width_)
    : val(val_), offset(offset_), width(width_)
  {}
};

class Select {
private:
  std::vector<ValueSlice> slices;

  bool many_slices = false;
  bool direct_value = false;
public:
  Select(ValueSlice &&slice_);
  Select(std::vector<ValueSlice> &&slices_);

  const std::vector<ValueSlice> & getSlices() const { return slices; }

};

class Input {
private:
  int width;
  std::optional<Select> select;
public:
  Input(int w)
    : width(w), select() {}

  bool isConnected() const { return select.has_value(); }
  void connect(Select && conn) { select = std::move(conn); }

  const Select& getSelect() const { return *select; }

  int getWidth() const { return width; }
};

class IFace {
private:
  std::vector<Value> outputs;
  std::vector<Input> inputs;
  std::vector<std::string> output_names;
  std::vector<std::string> input_names;

  std::unordered_map<std::string, Input *> input_lookup;
  std::unordered_map<std::string, Value *> output_lookup;

  bool is_definition;
public:
  IFace(const std::vector<std::pair<std::string, int>>& outputs,
        const std::vector<std::pair<std::string, int>>& inputs,
        bool is_defn);

  const std::vector<Value> & getOutputs() const { return outputs; }
  const std::vector<Input> & getInputs() const { return inputs; }

  const std::vector<std::string> & getOutputNames() const { return output_names; }
  const std::vector<std::string> & getInputNames() const { return input_names; }

  const Value * getOutput(const std::string &name) const;
  const Input * getInput(const std::string &name) const;
  Value * getOutput(const std::string &name);
  Input * getInput(const std::string &name);

  void print(const std::string &prefix = "") const;
};

class Definition;

class Instance {
private:
  std::string name;
  IFace interface;
  const Definition *defn;
public:
  Instance(const std::string &name_, 
           const std::vector<std::pair<std::string, int>>& outputs,
           const std::vector<std::pair<std::string, int>>& inputs,
           const Definition *defn);

  IFace & getIFace() { return interface; }
  const IFace & getIFace() const { return interface; }
  const std::string & getName() const { return name; }

  void print(const std::string &prefix = "") const;
};

class Definition {
private:
  std::string name;
  IFace interface;

  std::vector<Instance> instances;

  std::vector<const Instance *> pre_instances;
  std::vector<const Instance *> post_instances;
public:
  Definition(const std::string &name,
             const std::vector<std::pair<std::string, int>>& outputs,
             const std::vector<std::pair<std::string, int>>& inputs,
             std::vector<Instance> &&instances);

  Instance makeInstance(const std::string &name) const;

  const IFace & getIFace() const { return interface; }
  IFace & getIFace() { return interface; }
  const std::string & getName() const { return name; }

  void print(const std::string &prefix = "") const;
};

class Circuit {
private:
  std::list<Definition> definitions;
  Definition *top_defn;
public:
  Circuit(std::list<Definition>&& defns)
    : definitions(move(defns)),
      top_defn(&definitions.back())
  {}

  void print() const;
};

}

#endif
