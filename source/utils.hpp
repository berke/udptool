// utils.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#ifndef UTILS_HPP_20100521
#define UTILS_HPP_20100521

#include <inttypes.h>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <boost/shared_ptr.hpp>

#include "shorthands.hpp"

namespace utils
{
  template<typename T>
  void clear(T& t)
  {
    memset(&t, 0, sizeof(t));
  }

  inline void add_escaped(std::string& dst, const std::string &src)
  {
    dst += "\"";
    for(auto it = src.begin(); it != src.end(); it ++)
    {
      char c = *it;
      switch(c)
      {
        case '\\':
          dst += "\\\\";
          break;
        case '$':
          dst += "\\$";
          break;
        case '"':
          dst += "\\\"";
          break;
        case '\n':
          dst += "\\n";
          break;
        case '\r':
          dst += "\\r";
          break;
        case '\t':
          dst += "\\t";
          break;
        default:
          dst.push_back(c);
          break;
      }
    }
    dst += "\"";
  }

  template<typename T> void ignore_pointer(T *t) { }

  template<typename T> boost::shared_ptr<T> fixed_shared_ptr(T *t)
  {
    return boost::shared_ptr<T>(t, ignore_pointer<T>);
  }

  template<typename T>
  union alias
  {
    T element;
    char bytes[sizeof(T)];

    template<typename Array>
    void fill(Array a)
    {
      assert(a.size() >= sizeof(T));
      memcpy(bytes, a.data(), sizeof(T));
    }
  };

  class flag_saver
  {
    private:
      std::ostream& out;
      std::ios_base::fmtflags f;
    public:
      flag_saver(std::ostream& out) : out(out), f(out.flags()) { }
      ~flag_saver() { out.flags(f); }
  };


  struct hex32
  {
    uint32_t value;

    hex32() : value(0) { }

    hex32(uint32_t x) : value(x) { }

    bool operator==(const hex32& other)
    {
      return value == other.value;
    }

    bool operator!=(const hex32& other)
    {
      return value != other.value;
    }

    friend std::ostream& operator<<(std::ostream& out, const hex32& self)
    {
      flag_saver saver(out);
      out << std::setw(8) << std::setfill('0') << std::hex << self.value;
      return out;
    }
  };

  inline void hex_dump(std::ostream& out, const std::string& u, nat indent=0, bool wrap=true)
  {
    nat m = u.size();
    flag_saver saver(out);
    out << std::setfill('0') << std::hex;

    for(nat i = 0; i < m; i ++)
    {
      if((i & 15) == 0)
      {
        for(nat j = 0; j < indent; j ++)
        {
          out << " ";
        }
      }
      out << std::setw(2) << nat(u[i] & 255);
      if(wrap && (i == m - 1 || ((i & 15) == 15))) out << "\n";
      else out << " ";
    }
  }

  inline std::string read_string(std::istream& in, size_t n)
  {
    std::string u;
    u.reserve(n);
    char buf[256];

    while(n > 0)
    {
      size_t m = n > sizeof(buf) ? sizeof(buf) : n;
      in.read(buf, m);
      u.append(buf, m);
      n -= m;
    }

    return u;
  }

  template<typename T>
  std::string to_string(T& n)
  {
    std::stringstream u;
    u << n;
    return u.str();
  }
};

#endif
