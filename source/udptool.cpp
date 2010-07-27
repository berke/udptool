// udptool
//
// Author: Berke Durak <berke.durak@gmail.com>

#include <cmath>
#include <cfloat>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "boost_program_options_required_fix.hpp"
#include "microsecond_timer.hpp"
#include "link_statistic.hpp"
#include "rtclock.hpp"
#include "wprng.hpp"
#include "packet_header.hpp"
#include "no_check_socket_option.hpp"

namespace po = boost::program_options;
namespace as = boost::asio;
using boost::any;
using namespace std;

typedef unsigned int nat;

string to_string(nat& n)
{
  stringstream u;
  u << n;
  return u.str();
}

class packet_transmitter
{
  ofstream log; 
  uint64_t seq;
  rtclock clk;

public:
  packet_transmitter(const string& log_file) :
    log(log_file), seq(0)
  {
    log << "t_rx size seq" << endl;
  }

  virtual ~packet_transmitter() { } 

  void transmit(char *buffer, const size_t m0)
  {
    int64_t t_tx = clk.get();
    log << t_tx << " " << m0 << " " << seq << "\n";
    if(m0 < packet_header::encoded_size) return;
    size_t m = m0;
    packet_header ph(clk, m0 - packet_header::encoded_size, seq);
    stringstream s;
    ph.encode(s, m);
    wprng w(ph.check);
    for(nat i = 0; i < m; i ++)
    {
       s.put(w.get());
    }
    const string u = s.str();
    assert(m0 == u.size());
    memcpy(buffer, u.c_str(), m0);
    seq ++;
  }
};

class distribution
{
protected:
  void eat(istream& in, const char c)
  {
    char sep; in >> sep;
    if(sep != c) throw bad_parameters();
  }

  void check_eof(istream& in)
  {
    if(in.fail() || !in.eof()) throw bad_parameters();
  }

public:
  class bad_parameters { };
  typedef boost::shared_ptr<distribution> ptr;
  virtual ~distribution() { }
  virtual double next() = 0;
  virtual double mean() = 0;
};

class dirac : public distribution
{
  double x0;

public:
  typedef boost::shared_ptr<dirac> ptr;
  dirac(istream& in)
  {
    in >> x0;
    check_eof(in);
  }
  dirac(double x0_) : x0(x0_) { }
  double next() { return x0; }
  double mean() { return x0; }
};

class uniform : public distribution
{
  double x0, x1;

public:
  typedef boost::shared_ptr<uniform> ptr;
  uniform(istream& in)
  {
    in >> x0;
    eat(in, ',');
    in >> x1;
    check_eof(in);
  }
  uniform(double x0_, double x1_) : x0(x0_), x1(x1_) { }
  double next() { return x0 + (x1 - x0) * drand48(); }
  double mean() { return 0.5 * (x0 + x1); }
};

void validate(boost::any& v, 
              const std::vector<std::string>& values,
              distribution::ptr* target_type, int)
{
  const string& u = po::validators::get_single_string(values);

  size_t i0 = u.find(':');
  string kind = "dirac";
  if(i0 == string::npos)
  {
    // Assume Dirac by default
    i0 = -1;
  }
  else
  {
    kind = u.substr(0, i0);
  }
  const string param_s = u.substr(i0 + 1);
  stringstream param(param_s);
  distribution::ptr content;

  if(kind == "dirac")
  {
    try
    {
      content = dirac::ptr(new dirac(param));
    }
    catch(...)
    {
      throw po::validation_error("Bad Dirac distribution description");
    }
  }
  else if(kind == "uniform")
  {
    try
    {
      content = uniform::ptr(new uniform(param));
    }
    catch(...)
    {
      throw po::validation_error("Bad uniform distribution description");
    }
  }
  else
  {
    string u = "Unknown distribution kind ";
    u += kind;
    throw po::validation_error(u);
  }
  v = content;
}

class packet_receiver
{
  ofstream log; 
  uint64_t seq_min, seq_max, seq_last, out_of_order, count, decodable_count,
           byte_count, bad_checksum, truncated, total_errors, total_erroneous;
  int64_t t_first, t_last;
  rtclock clk;

public:
  packet_receiver(const string& log_file) :
    log(log_file), seq_min(0), seq_max(0), seq_last(0), out_of_order(0),
    count(0), decodable_count(0), byte_count(0), bad_checksum(0), truncated(0),
    total_errors(0), total_erroneous(0)
  {
    log << "t_rx size status seq t_tx errors" << endl;
  }

  virtual ~packet_receiver() { } 

  void receive(const char *buffer, const size_t m0)
  {
    const int64_t t_rx = clk.get();
    string status = "ok";
    uint32_t seq = 0;
    uint64_t t_tx = 0;
    uint32_t errors = 0;

    const string u(buffer, m0);
    size_t m = m0;
    stringstream s(u, ios_base::in);
     
    do
    {
      if(m0 < sizeof(packet_header))
      {
        status = "short";
        break;
      }

      if(!count)
      {
        t_first = t_rx;
      }
      t_last = t_rx;

      try
      {
        packet_header ph(s, m);

        if(!ph.checksum_valid())
        {
          status = "bad";
          bad_checksum ++;
          break;
        }

        seq = ph.sequence;
        if(!count || seq < seq_min) seq_min = seq;
        if(!count || seq > seq_max) seq_max = seq;
        if(count && seq != seq_last + 1) out_of_order ++;
        seq_last = seq;

        t_tx = ph.timestamp;
        wprng w(ph.check);

        if(ph.size != m)
        {
          truncated ++;
          status = "trunc";
          break;
        }
        
        decodable_count ++;

        for(nat i = 0; i < m; i ++)
        {
          uint8_t expected_byte, received_byte;

          expected_byte = w.get();
          received_byte = s.get();

          if(expected_byte != received_byte) errors ++;
        }
        if(errors > 0)
        {
          total_erroneous ++;
          total_errors += errors;
        }
      }
      catch(packet_header::encoding_error& e)
      {
        status = "bad";
      }
    }
    while(false);

    byte_count += m0;
    count ++;

    log << t_rx << " " << m-0 << " " << status << " " << seq << " " << t_tx << " " << errors << "\n";
  }

  void output(ostream& out) const
  {
    if(!count)
    {
      out << "No packets received";
      return;
    }

    double dt = (t_last - t_first)/1e6;

    out <<
      "RX statistics:\n"
      "  Total packets ............................ " << count                  << " pk\n"
      "  Total bytes .............................. " << byte_count             << " B\n"
      "  Time ..................................... " << dt                     << " s\n"
      "  Packet rate .............................. " << count / dt             << " pk/s\n"
      "  Bandwidth ................................ " << 8e-6 * byte_count / dt << " Mbit/s\n"
      "  Packets with bad checksum ................ " << bad_checksum           << " pk\n"
      "  Truncated packets ........................ " << truncated              << " pk\n"
      "  Highest sequence # ....................... " << seq_min                << "\n"
      "  Lowest sequence # ........................ " << seq_max                << "\n"
      "  Out of order packets ..................... " << out_of_order           << " pk\n"
      "  Decodable packets ........................ " << decodable_count        << " pk\n"
      "  Decodable loss ratio ..................... " << 1 - (double(decodable_count) / (seq_max - seq_min + 1)) << "\n"
      "  Payload byte errors ...................... " << total_errors           << " B\n"
      "  Decodables with erroneous payloads........ " << total_erroneous        << " pk"
    ;
  }

  friend ostream& operator<<(ostream& out, const packet_receiver& self)
  {
    self.output(out);
    return out;
  }

};

int main(int argc, char* argv[]) //{{{
{
  typedef const char *option;
  
  string s_ip = "0.0.0.0";
  nat s_port = 0;
  string d_ip;
  nat d_port = 33333;
  nat count = 0;
  bool verbose = false;
  string log_file = "tx.log";
  double bandwidth = 0;
  double detailed_every = 0;
  nat avg_window = 10000, max_window = 10000;
#if HAVE_SO_NO_CHECK
  bool no_check = true;
#endif

  po::options_description desc("Available options");
  desc.add_options()
    ("tx",            po::bool_switch(&transmit),                       "Transmit packets")
    ("rx",            po::bool_switch(&receive),                        "Receive packets")
    ("help,h",                                                          "Display this information")
    ("sip",           po::value<string>(&s_ip),                         "Source IP to bind to")
    ("sport",         po::value<nat>(&s_port) bpo_required,             "Source port to bind to")
    ("dip",           po::value<string>(&d_ip) bpo_required,            "Destination IP to transmit to")
    ("dport",         po::value<nat>(&d_port) bpo_required,             "Destination port to transmit to")
    ("size",          po::value< vector<distribution::ptr> >() bpo_required,
                                                                        "Add a packet size distribution")
    ("delay",         po::value< vector<distribution::ptr> >() bpo_required,
                                                                        "Add a packet transmission delay distribution (ms)")
    ("bandwidth",     po::value<double>(&bandwidth),                    "Adjust delay or packet size to bandwidth (Mbit/s)") 
    ("count",         po::value<nat>(&count),                           "Number of packets to send, or 0 for no limit)")
    ("verbose",       po::bool_switch(&verbose),                        "Display each packet as it is sent")
    ("log-file",      po::value<string>(&log_file),                     "Log file")
    ("avg-window",    po::value<nat>(&avg_window),                      "Size of running average window in packets")
    ("max-window",    po::value<nat>(&max_window),                      "Size of maximum window in packets")
    ("detailed-every", po::value<double>(&detailed_every),        "Display detailed statistics every so many seconds")
#if HAVE_SO_NO_CHECK
    ("no-check",      po::bool_switch(&no_check),                       "Disable UDP checksumming")
#endif
  ;

  enum
  {
    display_delay_microseconds = 1000000,
    display_every = 100,
    default_size  = 1472,
    default_delay = 1
  };

  try
  {
    // Parse command-line options

    po::variables_map vm;
    po::store(
        po::command_line_parser(argc, argv).options(desc).run(),
        vm);
    po::notify(vm);

    if(vm.count("help"))
    {
      cout << desc << endl;
      return 1;
    }

    if(d_ip.empty())
    {
      cerr << "Error: no destination IP" << endl;
      return 1;
    }
    as::io_service io;

    using as::ip::udp;

    {
      // Resolve destination address
      cout << "Resolving " << d_ip << " port " << d_port << endl;
      udp::resolver resolver(io);
      udp::resolver::query query(udp::v4(), d_ip, to_string(d_port));
      udp::endpoint receiver_endpoint = *resolver.resolve(query);

      cout << "Opening socket" << endl;
      udp::endpoint src(as::ip::address::from_string(s_ip), s_port);
      udp::socket socket(io, src);
#if HAVE_SO_NO_CHECK
      if(no_check)
      {
        cout << "Disabling UDP checksumming" << endl;
        as::socket_base_extra::no_check opt(false);
        boost::system::error_code ec;
        socket.set_option(opt, ec);
        if(ec)
        {
          string u = "Cannot set NO_CHECK option: ";
          u += ec.message();
          throw runtime_error(u);
        }
      }
#endif

      packet_transmitter tx(log_file);

      nat sent = 0;
      microsecond_timer::microseconds t_last = microsecond_timer::get();
      size_t bytes = 0;

      link_statistic stat(avg_window, max_window);

      cout << "Starting flood" << endl;
      boost::asio::deadline_timer t(io);

      vector<distribution::ptr> sizes, delays;

      {
        po::variable_value size_v = vm["size"],
                           delay_v = vm["delay"];
        if(!size_v.empty()) sizes = size_v.as< vector<distribution::ptr> >();
        if(!delay_v.empty()) delays = delay_v.as< vector<distribution::ptr> >();
      }

      vector<distribution::ptr>::iterator
        d_it = delays.begin(),
        s_it = sizes.begin();

      bool have_delays = d_it != delays.end(),
           have_sizes  = s_it != sizes.end();
      
      if(!have_delays && !have_sizes && bandwidth == 0)
      {
        cerr << "Error: no delay, size nor bandwidth specified" << endl;
        return 1;
      }
      if((!have_delays || !have_sizes) && bandwidth == 0)
      {
        cerr << "Error: no bandwidth speicifed" << endl;
        return 1;
      }
      if(bandwidth > 0 && have_delays && have_sizes)
      {
        cerr << "Error: cannot specify all three of bandwidth, delays and sizes." << endl;
        return 1;
      }

      microsecond_timer::microseconds t0 = microsecond_timer::get();

      while(count == 0 || sent < count)
      {
        if(sent > 0 && sent % display_every == 0)
        {
          microsecond_timer::microseconds t_now = microsecond_timer::get();
          if(t_now - t_last >= display_delay_microseconds)
          {
            cout << "Sent: " << stat << endl;
            t_last = t_now;
          }
        }
        sent ++;

        double delay, delay_avg;
        size_t size, size_avg;

        if(have_delays)
        {
          delay = (*d_it)->next();
          delay_avg = (*d_it)->mean();
          d_it ++;
          if(d_it == delays.end()) d_it = delays.begin();
        }
        else
        {
          delay_avg = delay = default_delay;
        }

        if(have_sizes)
        {
          size = (*s_it)->next();
          size_avg = (*s_it)->mean();
          s_it ++;
          if(s_it == sizes.end()) s_it = sizes.begin();
        }
        else
        {
          size_avg = size = default_size;
        }

        if(!have_delays)
        {
          delay = 1e3 * double(size_avg) / (1e6/8.0 * bandwidth);
        }

        if(!have_sizes)
        {
          size = double(delay_avg) * 1e-3 * (1e6/8.0 * bandwidth);
        }

        if(size <= 0) continue;

        std::vector<char> buf(size);
        tx.transmit(buf.data(), size);

        if(delay > 0)
          t.expires_at(
            microsecond_timer::as_posix(t0 + sent * delay * 1e3)
          );
        socket.send_to(boost::asio::buffer(buf), receiver_endpoint);
        if(verbose) cerr << size << " " << delay << endl;
        bytes += size;
        stat.add(size);
        if(delay > 0) t.wait();
      }
      cout << "Total: " << stat << endl;
    }
  }
  catch(po::error& e)
  {
    cerr << "Error: " << e.what() << endl;
    cerr << desc << endl;
    return 1;
  }
  catch(exception& e)
  {
    cerr << "Error: " << e.what() << endl;
    return 1;
  }
  catch(...)
  {
    cerr << "Error: Unknown error." << endl;
    return 2;
  }

  return 0;
} //}}}