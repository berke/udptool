// packet_header.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#ifndef PACKET_HEADER_HPP_20100721
#define PACKET_HEADER_HPP_20100721o

#include <iostream>
#include "network_word.hpp"

struct packet_header
{
  uint16_t sequence;
  uint32_t timestamp;
  uint16_t size;
  uint16_t check;

  enum { encoded_size = 10 };

  class encoding_error { };

  packet_header(rtclock& rtc, uint32_t size_, uint32_t sequence_) :
    sequence(sequence_),
    timestamp(rtc.get()),
    size(size_),
    check(get_checksum())
  {
  }

  packet_header(std::istream& in, size_t &m)
  {
    using namespace network_word;
    try
    {
      netword::read(in, sequence, m);
      netword::read(in, timestamp, m);
      netword::read(in, size, m);
      netword::read(in, check, m);
    }
    catch(network_word::bad_encoding& e)
    {
      throw encoding_error();
    }
  }

  void encode(std::ostream& out, size_t &m)
  {
    using namespace network_word;
    try
    {
      netword::write(out, sequence, m);
      netword::write(out, timestamp, m);
      netword::write(out, size, m);
      netword::write(out, check, m);
    }
    catch(network_word::bad_encoding& e)
    {
      throw encoding_error();
    }
  }
  
  uint32_t get_checksum() const
  {
    return ~(sequence ^ size ^ timestamp);
  }

  bool checksum_valid() const
  {
    return get_checksum() == check;
  }

  friend std::ostream& operator<<(std::ostream& out, const packet_header& self)
  {
    out << "pkg{s=" << self.sequence << " t=" << self.timestamp << " c=" << (self.checksum_valid() ? "ok" : "bad") << "}";
    return out;
  }
};

#endif
