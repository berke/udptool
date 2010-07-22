// microsecond_timer.cpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#include <boost/date_time/gregorian/gregorian.hpp>

#include "microsecond_timer.hpp"

namespace microsecond_timer
{
  namespace pt = boost::posix_time;

  const pt::ptime t0(pt::microsec_clock::universal_time());

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
