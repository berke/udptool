// link_statistic.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>
// vim:set ts=2 sw=2 foldmarker={,}:

#ifndef LINK_STATISTIC_HPP_20100521
#define LINK_STATISTIC_HPP_20100521

#include <iostream>

#include <set>
#include <boost/circular_buffer.hpp>
#include <boost/shared_ptr.hpp>

#include "shorthands.hpp"
#include "microsecond_timer.hpp"

/// \brief Maintain aggregate link statistics.
/// This class computes a running bandwidth average, used for reporting.
class link_statistic
{
  nat    count;
  size_t total;
  microsecond_timer::microseconds start;

  struct item
  {
    size_t size;
    microsecond_timer::microseconds when;

    explicit item(size_t size_, microsecond_timer::microseconds when_) : size(size_), when(when_) { }
  };
  boost::circular_buffer<item> items; // For running average bandwidth computation
  size_t buffer_total;

  boost::circular_buffer<double> max_circular;
  std::multiset<double> max_set;

public:
  typedef boost::shared_ptr<link_statistic> ptr;
  
  /// Construct a link statistic computing object
  /// \param running_average_window Compute the average bandwidth over the last this many samples
  /// \param maximum_window         Compute the maximum bandwidth over the last this many samples
  link_statistic(nat running_average_window=10, nat maximum_window=100);

  /// Notify that a packet has been transmitted.
  /// \param size The size of the packet in bytes
  /// \param t    The time at which the packet has been transmitted (defaults to now)
  void add(size_t size, microsecond_timer::microseconds t=microsecond_timer::get());

  /// Return the average bandwidth.
  /// \returns Average bandwidth over the specificed number of samples, in kB/s
  double average_bandwidth() const;

  /// Return the duration over which the average has been computed.
  double average_duration() const;

  /// Return the maximum bandwidth.
  /// \returns Maximum bandwidth over the specificed number of samples, in kB/s
  double max_bandwidth() const;

  friend std::ostream& operator<<(std::ostream& out, const link_statistic& self);
};

#endif
