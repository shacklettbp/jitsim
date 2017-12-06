#ifndef JITSIM_UTILS_HPP_INCLUDED
#define JITSIM_UTILS_HPP_INCLUDED

namespace JITSim {
  template<class T> static T& condDeref(T& t) { return t; }
  template<class T> static T& condDeref(T*& t) { return *t; }
  template<class T> static T& condDeref(T* t) { return *t; }

  inline int getNumBytes(int bits) {
    if (bits % 8 == 0) {
      return bits / 8;
    } else {
      return bits / 8 + 1;
    }
  }
}

#endif
