// udprecv
//
// Author: Berke Durak <berke.durak@gmail.com>

#include <cmath>
#include <cfloat>
#include <cctype>
#include <cstdlib>
#include <ctime>
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
  nat s_port = 33333;
  nat count = 0;
  size_t size = 50000;
  string log_file = "rx.log";
  double detailed_every = 0;

  po::options_description desc("Available options");
  desc.add_options()
    ("help,h",                                                   "Display this information")
    ("sip",            po::value<string>(&s_ip),                  "Source IP to bind to")
    ("sport",          po::value<nat>(&s_port) bpo_required,      "Source port to bind to")
    ("count",          po::value<nat>(&count),                    "Number of packets to receive, or 0 for no limit)")
    ("size",           po::value<size_t>(&size),                  "Reception buffer size")
    ("log-file",       po::value<string>(&log_file),              "Log file")
    ("detailed-every", po::value<double>(&detailed_every),        "Display detailed statistics every so many seconds")
  ;

  enum { display_delay_microseconds = 1000000,
         display_every = 100 };

  try
  {
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

    as::io_service io;

    using as::ip::udp;

    {
      boost::system::error_code ec;

      cout << "Opening socket" << endl;
      udp::endpoint src(as::ip::address::from_string(s_ip), s_port);
      udp::socket socket(io, src);

#if HAVE_SO_NO_CHECK
      as::socket_base_extra::no_check opt(false);
      socket.set_option(opt, ec);
      if(ec)
      {
        string u = "Cannot set NO_CHECK option: ";
        u += ec.message();
        throw runtime_error(u);
      }
#endif

      link_statistic stat(1000, 1000);

      cout << "Listening" << endl;

      nat received = 0;
      vector<char> buf(size);
      packet_receiver rx(log_file);
      microsecond_timer::microseconds t_last = microsecond_timer::get(), t_last_detailed = t_last;

      while(count == 0 || received < count)
      {
        udp::endpoint remote;
        size_t size = socket.receive_from(boost::asio::buffer(buf), remote, 0, ec);
        if(ec)
        {
          cout << "Reception error: " << ec.message() << endl;
        }
        else
        {
          stat.add(size);
          rx.receive(buf.data(), size);
          received ++;
        }

        if(received % display_every == 0)
        {
          microsecond_timer::microseconds t_now = microsecond_timer::get();
          if(t_now - t_last >= display_delay_microseconds)
          {
            cout << "Received: " << stat << endl;
            t_last = t_now;
          }
          if(detailed_every > 0 && t_now - t_last_detailed >= detailed_every * 1e6)
          {
            cout << rx << endl;
            t_last_detailed = t_now;
          }
        }
      }
      cout << "Finally: " << stat << endl;
      cout << rx << endl;
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
