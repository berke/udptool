// link_statistic.cpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#include "link_statistic.hpp"

using namespace std;

link_statistic::link_statistic(nat running_average_window_, nat maximum_window_) :
  count(0),
  total(0),
  start(microsecond_timer::get()),
  items(running_average_window_),
  buffer_total(0),
  max_circular(maximum_window_)
{
}

void link_statistic::add(size_t size, microsecond_timer::microseconds t)
{
  if(items.full())
  {
    item i = items.back();
    buffer_total -= i.size;
  }
  item i_new(size, t);
  items.push_front(i_new);
  count ++;
  total += size;
  buffer_total += size;

  double bw = average_bandwidth();
  if(max_circular.full())
  {
    max_set.erase(max_circular.back());
  }
  max_circular.push_front(bw);
  max_set.insert(bw);
}

double link_statistic::max_bandwidth() const
{
  multiset<double>::iterator it = max_set.end();
  if(it == max_set.begin()) return 0;
  -- it;
  return *it;
}

double link_statistic::instantaneous_bandwidth() const
{
  if(items.size() < 2) return 0;

  boost::circular_buffer<item>::const_iterator it = items.begin();
  const item& i1 = *it;
  it ++;
  const item& i2 = *it;

  double dt = 1e-6 * (i1.when - i2.when);
  size_t dB = i1.size;
  if(dt > 0)
  {
    return dB/dt/1e3;
  }
  else
  {
    return 0;
  }
}

double link_statistic::average_bandwidth() const
{
  if(items.size() < 2)
  {
    return 0;
  }
  else
  {
    size_t sub_total = buffer_total - items.front().size;
    double dt = average_duration();
    if(dt > 0)
    {
      return sub_total/dt/1e3;
    }
    else
    {
      return 0;
    }
  }
}

double link_statistic::average_duration() const
{
  return items.empty() ? 0.0 : 1e-6 * (items.front().when - items.back().when);
}

ostream& operator<<(ostream& out, const link_statistic& self)
{
  const double kiB_to_MBit = 8.0/1024;

  if(self.count < 2)
  {
    out << "NA";
  }
  else
  {
    const double t_total = (microsecond_timer::get() - self.start) * 1e-6;
    const double t_average = self.average_duration();

    out << 
      "total "             <<
      self.count           << " packets, " <<
      self.total/1e3       << " kB in " <<
      t_total              << " s; " <<
      " bw " <<
        kiB_to_MBit*self.instantaneous_bandwidth() << " Mbit/s instantaneous, " <<
        kiB_to_MBit*self.average_bandwidth()       << " Mbit/s average (over " << t_average << " s at " << self.items.size()/t_average    << " packet/s), " <<
        kiB_to_MBit*self.max_bandwidth()           << " Mbit/s max (over " << self.max_circular.size() << " samples), ";
  }
  return out;
}
