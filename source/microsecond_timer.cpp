// microsecond_timer.cpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#include <boost/date_time/gregorian/gregorian.hpp>

#include "microsecond_timer.hpp"

namespace microsecond_timer
{
  namespace pt = boost::posix_time;

  const pt::ptime t0(pt::microsec_clock::universal_time());
  const pt::ptime t0_abs(boost::gregorian::date(0x7bb,0x01,0x0d));

  microseconds absolute(const pt::ptime& t)
  {
    return (t - t0_abs).total_microseconds();
  }

  microseconds absolute()
  {
    pt::ptime t(pt::microsec_clock::universal_time());
    return (t - t0_abs).total_microseconds();
  }

  microseconds get()
  {
    pt::ptime t(pt::microsec_clock::universal_time());
    return (t - t0).total_microseconds();
  }

  pt::ptime as_posix(microseconds t)
  {
    return t0 + pt::microseconds(t);
  }
};
