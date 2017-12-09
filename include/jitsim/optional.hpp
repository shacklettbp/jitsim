#ifndef JITSIM_OPTIONAL_HPP_INCLUDED
#define JITSIM_OPTIONAL_HPP_INCLUDED

#include <cassert>
#include <utility>

namespace JITSim {

template <class T>
class optional 
{
private:
  bool has_value_;
  union { T object_; bool missing_; };

public:
  /* constructor for uninitialized optional */
  /* missing_ field of union is necessary to get rid of "may be used uninitialized" warning */
  optional() : has_value_(false), missing_() {}

  optional(const T & other) : has_value_(true), object_(other) {}

  /* constructor for initialized optional */
  optional(T && other) : has_value_(true), object_(std::move(other)) {}

  /* move constructor */
  optional(optional<T> && other)
    : has_value_(other.has_value_)
  {
    if (has_value_) {
      new(&object_) T(std::move(other.object_));
    }
  }

  /* copy constructor */
  optional(const optional<T> & other)
    : has_value_(other.has_value_)
  {
    if (has_value_) {
      new(&object_) T(other.object_);
    }
  }

  /* initialize in place */
  template <typename... Targs>
  void initialize(Targs&&... Fargs)
  {
    assert(not has_value());
    new(&object_) T(std::forward<Targs>(Fargs)...);
    has_value_ = true;
  }

  /* move assignment operators */
  const optional & operator=(optional<T> && other)
  {
    /* destroy if necessary */
    if (has_value_) {
      object_.~T();
    }

    has_value_ = other.has_value_;
    if ( has_value_ ) {
      new(&object_) T(std::move(other.object_));
    }
    return *this;
  }

  /* copy assignment operator */
  const optional & operator=(const optional<T> & other)
  {
    /* destroy if necessary */
    if (has_value_) {
      object_.~T();
    }

    has_value_ = other.has_value_;
    if (has_value_) {
      new(&object_) T( other.object_ );
    }
    return *this;
  }

  /* equality */
  bool operator==(const optional<T> & other) const
  {
    if (has_value_) {
      return other.has_value_ ? (get() == other.get()) : false;
    } else {
      return !other.has_value_;
    }
  }

  bool operator!=(const optional<T> & other) const
  {
    return not operator==(other);
  }

  /* getters */
  bool has_value() const { return has_value_; }
  const T & get() const { assert(has_value()); return object_; }

  T & get() { assert(has_value()); return object_; }

  const T & operator*() const { return object_; }
  T & operator*() { return object_; }

  const T * operator->() const { return &object_; }
  T * operator->() { return &object_; }

  /* destructor */
  ~optional() { if ( has_value() ) { object_.~T(); } }
};

}

#endif
