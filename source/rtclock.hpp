// rtclock.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#ifndef RTCLOCK_HPP_20100721
#define RTCLOCK_HPP_20100721

#include <time.h>
#include <stdexcept>

class rtclock
{
  struct timespec ts0;

  void get(struct timespec& ts)
  {
    if(clock_gettime(CLOCK_MONOTONIC, &ts))
    {
      throw std::runtime_error("Cannot get CLOCK_MONOTONIC_RAW clock");
    }
  }

public:
  rtclock(bool absolute=false)
  {
    if(absolute)
    {
      ts0.tv_sec = 0;
      ts0.tv_nsec = 0;
    }
    else
    {
      get(ts0);
    }
  }

  int64_t get()
  {
    struct timespec ts;
    get(ts);
    int64_t dt = 1000000 * (int64_t) (ts.tv_sec - ts0.tv_sec);
    dt += (ts.tv_nsec - ts0.tv_nsec) / 1000;
    return dt;
  }
};

#endif
