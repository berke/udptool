// microsecond_timer.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#ifndef MICROSECOND_TIMER_HPP_20100525
#define MICROSECOND_TIMER_HPP_20100525

#include <boost/date_time/posix_time/posix_time.hpp>

#include "shorthands.hpp"

namespace microsecond_timer
{
  typedef int64_t microseconds;

  /// Return relative time in microseconds
  microseconds get();

  /// Convert a POSIX time to absolute microseconds
  microseconds from_posix(const boost::posix_time::ptime& t);

  // Convert a time in seconds to microseconds
  // \param t Time in seconds
  static inline microseconds from_seconds(double t) { return t * 1e6; };

  // Convert microsecond absolute time to POSIX
  boost::posix_time::ptime as_posix(microseconds t);
};

#endif
