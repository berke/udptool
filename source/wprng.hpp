// wprng.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#ifndef WPRNG_HPP_20100721
#define WPRNG_HPP_20100721

class wprng
{
  uint32_t a, b, c, d;

  static inline uint32_t rol32(uint32_t x, uint32_t y)
  {
    y &= 31;
    return (x << y) | (x >> (32 - y));
  }

public:
  wprng(uint32_t seed=0)
  {
    a = 0xdeadbeef ^ seed;
    b = 0x0badcafe + seed;
    c = 0xdeadface - seed;
    d = 0xdefaced1 ^ seed;
  }

  void step()
  {
    a = rol32(a, d);
    b ^= 0x89abcdef;
    c = rol32(c, b);
    d = rol32(d, a);
    c ^= 0x31415926;
    a ^= 0x01234567;
    b = rol32(b, c);
    a += c;
    b ^= d;
    c -= a;
    d -= b;
    d ^= 0x54581414;
  }

  uint32_t get()
  {
    step();
    return a;
  }
};

#endif
