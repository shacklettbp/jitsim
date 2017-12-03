#ifndef JITSIM_CIRCUIT_HPP_INCLUDED
#define JITSIM_CIRCUIT_HPP_INCLUDED

#include <jitsim/simanalysis.hpp>
#include <jitsim/primitive.hpp>

#include <cassert>
#include <unordered_map>
#include <string>
#include <iostream>
#include <vector>
#include <deque>
#include <utility>
#include <functional>
#include <optional>

#include <llvm/ADT/APInt.h>

namespace JITSim {

class IFace;
class Definition;
class Instance;

class Source {
private:
  std::string name;
  int width;
public:
  Source(const std::string &name_, int w)
    : name(name_), width(w)
  {}

  Source(const Source &) = delete;
  Source(Source &&) = default;

  const std::string & getName() const { return name; }
  int getWidth() const { return width; }
};

class SourceSlice {
private:
  const Definition *definition;
  const Instance *instance;
  const IFace *iface;
  const Source *val;
  int offset;
  int width;
  std::optional<llvm::APInt> constant;

public:
  SourceSlice(const Definition *definition_, const Instance *instance_,
             const Source *val_, int offset_, int width_);

  SourceSlice(const std::vector<bool> &constant_);

  const Definition * getDefinition() const { return definition; }
  const Instance * getInstance() const { return instance; }
  const IFace * getIFace() const { return iface; }
  const Source * getSource() const { return val; }
  const llvm::APInt & getConstant() const { return *constant; }

  int getEndIdx() const { return offset + width; }
  int getWidth() const { return width; }
  int getOffset() const { return offset; }
  bool isWhole() const { return offset == 0 && width == val->getWidth(); }
  bool isConstant() const { return constant.has_value(); }
  bool isDefinitionAttached() const { return !!definition; }
  bool isInstanceAttached() const { return !!instance; }

  void extend(const SourceSlice &other);

  std::string repr() const;
};

class Select {
private:
  std::vector<SourceSlice> slices;

  bool has_many_slices = true;

  const SourceSlice *direct_source = nullptr;

  void compressSlices();
public:
  Select(SourceSlice &&slice_);
  Select(std::vector<SourceSlice> &&slices_);

  const std::vector<SourceSlice> & getSlices() const { return slices; }

  bool isDirect() const { return direct_source != nullptr; }
  const SourceSlice & getDirect() const { return *direct_source; }

  std::string repr() const;
};

class Sink {
private:
  std::string name;
  int width;
  std::optional<Select> select;
public:
  Sink(const std::string &name_, int w)
    : name(name_), width(w), select() {}

  Sink(const Sink &) = delete;
  Sink(Sink &&) = default;

  bool isConnected() const { return select.has_value(); }
  void connect(Select &&conn) { select = std::move(conn); }

  const Select & getSelect() const { return *select; }

  const std::string & getName() const { return name; }
  int getWidth() const { return width; }
};

class ClkSource {
private:
  std::string name;
public:
  ClkSource(const std::string &name_)
    : name(name_)
  {}

  const std::string & getName() const { return name; }
};

class ClkSink {
private:
  std::string name;
  const ClkSource *source;
public:
  ClkSink(const std::string &name_, const ClkSource *source_)
    : name(name_), source(source_)
  {}

  const std::string & getName() const { return name; }

  bool isConnected() const { return source != nullptr; }
  void connect(const ClkSource *source_) { source = source_; }
};

class IFace {
private:
  std::string name;
  std::vector<Sink> sinks;
  std::vector<Source> sources;
  std::vector<ClkSink> clk_sinks;
  std::vector<ClkSource> clk_sources;
  std::unordered_map<std::string, Sink *> sink_lookup;
  std::unordered_map<std::string, Source *> source_lookup;
  bool is_definition;
public:
  IFace(const std::string &name_,
        std::vector<Sink> &&sinks_,
        std::vector<Source> &&sources_,
        std::vector<ClkSink> &&clk_sinks,
        std::vector<ClkSource> &&clk_sources,
        const bool is_definition_);

  IFace(const IFace &) = delete;
  IFace(IFace &&) = default;

  const std::string & getName() const { return name; }
  const std::vector<Source> & getSources() const { return sources; }
  const std::vector<Sink> & getSinks() const { return sinks; }
  std::vector<Source> & getSources() { return sources; }
  std::vector<Sink> & getSinks() { return sinks; }

  const std::vector<ClkSource> & getClkSources() const { return clk_sources; }
  const std::vector<ClkSink> & getClkSinks() const { return clk_sinks; }

  const Source * getSource(const std::string &name) const;
  const Sink * getSink(const std::string &name) const;
  Source * getSource(const std::string &name);
  Sink * getSink(const std::string &name);

  void print(const std::string &prefix = "") const;
  void print_connectivity(const std::string &prefix = "") const;

  bool isDefinition() const { return is_definition; }
  bool isInstance() const { return !is_definition; }
};

class InstanceIFace : public IFace {
private:
  std::unordered_map<const Source *, const Sink *> defn_source_to_sink;

public:
  InstanceIFace(const std::string &name_, const IFace &defn_iface);

  const Sink * getSink(const Source *src) const 
  {
    return defn_source_to_sink.find(src)->second;
  }
};

class Instance {
private:
  std::string name;
  InstanceIFace interface;
  const Definition *defn;
public:
  Instance(const std::string &name_, 
           const Definition *defn);

  InstanceIFace & getIFace() { return interface; }
  const InstanceIFace & getIFace() const { return interface; }
  const SimInfo & getSimInfo() const;
  const Definition & getDefinition() const { return *defn; }
  const std::string & getName() const { return name; }

  void print(const std::string &prefix = "") const;
};

class Definition {
private:
  std::string name;
  std::string safe_name;
  IFace interface;

  std::vector<Instance> instances;

  SimInfo siminfo;
public:
  Definition(const std::string &name,
             IFace &&interface,
             std::vector<Instance> &&instances,
             std::function<void (Definition&, std::vector<Instance> &instances)> make_connections);

  Definition(const std::string &name,
             IFace &&interface,
             const Primitive &primitive);

  Instance makeInstance(const std::string &name) const;

  const IFace & getIFace() const { return interface; }
  IFace & getIFace() { return interface; }
  const std::string & getName() const { return name; }
  const std::string & getSafeName() const { return safe_name; }
  const SimInfo & getSimInfo() const { return siminfo; }

  void print(const std::string &prefix = "") const;
};

class Circuit {
private:
  std::deque<Definition> definitions;
  Definition *top_defn;
public:
  Circuit(std::deque<Definition>&& defns)
    : definitions(move(defns)),
      top_defn(&definitions.back())
  {}

  void print() const;

  const std::deque<Definition>& getDefinitions() const { return definitions; }
  const Definition& getTopDefinition() const { return *top_defn; }
};

}

#endif
