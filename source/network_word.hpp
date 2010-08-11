// network_word.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>
// vim:set ts=2 sw=2 foldmarker={,}:

#ifndef NETWORK_WORD_HPP_20100721
#define NETWORK_WORD_HPP_20100721

#include <endian.h>
#include <iostream>

namespace network_word
{
  class bad_encoding { };

  struct word16
  {
    typedef uint16_t t;
    static t to_net(t x) { return htobe16(x); }
    static t from_net(t x) { return be16toh(x); }
  };

  struct word32
  {
    typedef uint32_t t;
    static t to_net(t x) { return htobe32(x); }
    static t from_net(t x) { return be32toh(x); }
  };

  struct word64
  {
    typedef uint64_t t;
    static t to_net(t x) { return htobe64(x); }
    static t from_net(t x) { return be64toh(x); }
  };

  template<class Word>
  struct word_io
  {
    typedef typename Word::t t;

    static void write(std::ostream& out, t x, size_t &m)
    {
      typename Word::t y;
      if(m < sizeof(y)) throw bad_encoding();
      char *y_alias;
      y = Word::to_net(x);
      y_alias = reinterpret_cast<char *>(&y);
      out.write(y_alias, sizeof(y));
      m -= sizeof(y);
    }

    static void read(std::istream& in, t &x, size_t &m)
    {
      t y;
      char *y_alias;
      if(m < sizeof(y)) throw bad_encoding();
      y_alias = reinterpret_cast<char *>(&y);
      in.read(y_alias, sizeof(y));
      m -= sizeof(y);
      x = Word::from_net(y);
    }
  };

  struct netword
  {
    static void write(std::ostream& out, uint16_t x, size_t &m)  { word_io<word16>::write(out, x, m); }
    static void write(std::ostream& out, uint32_t x, size_t &m)  { word_io<word32>::write(out, x, m); }
    static void write(std::ostream& out, uint64_t x, size_t &m)  { word_io<word64>::write(out, x, m); }
    static void read(std::istream& in,   uint16_t &x, size_t &m) { return word_io<word16>::read(in, x, m); }
    static void read(std::istream& in,   uint32_t &x, size_t &m) { return word_io<word32>::read(in, x, m); }
    static void read(std::istream& in,   uint64_t &x, size_t &m) { return word_io<word64>::read(in, x, m); }
    static void ignore(std::istream& in, size_t n, size_t &m)
    {
      if(m < n) throw bad_encoding();
      in.ignore(n);
      m -= n;
    }
    static void check_eof(size_t &m)
    {
      if(m != 0) throw bad_encoding();
    }
  };

};

#endif
