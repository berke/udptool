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
  uint64_t seq;
  rtclock clk;

public:
  packet_receiver(const string& log_file) :
    log(log_file), seq(0)
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

      try
      {
        packet_header ph(s, m);

        if(ph.checksum_valid())
        {
          status = "bad";
          break;
        }

        seq = ph.sequence;
        t_tx = ph.timestamp;

        wprng w(ph.check);

        if(ph.size != m)
        {
          status = "trunc";
          break;
        }

        for(nat i = 0; i < m; i ++)
        {
          uint8_t expected_byte, received_byte;

          expected_byte = w.get();
          received_byte = s.get();

          if(expected_byte != received_byte) errors ++;
        }
      }
      catch(packet_header::encoding_error& e)
      {
        status = "bad";
      }
    }
    while(false);

    log << t_rx << " " << m-0 << " " << status << " " << seq << " " << t_tx << " " << errors << "\n";
  }
};

int main(int argc, char* argv[]) //{{{
{
  typedef const char *option;
  
  string s_ip = "0.0.0.0";
  nat s_port = 5000;
  nat count = 0;
  size_t size = 1500;
  string log_file = "pkt.log";

  po::options_description desc("Available options");
  desc.add_options()
    ("help,h",                                                   "Display this information")
    ("sip",           po::value<string>(&s_ip),                  "Source IP to bind to")
    ("sport",         po::value<nat>(&s_port) bpo_required,      "Source port to bind to")
    ("count",         po::value<nat>(&count),                    "Number of packets to receive, or 0 for no limit)")
    ("size",          po::value<size_t>(&size),                  "Reception buffer size")
    ("log-file",      po::value<string>(&log_file),              "Log file")
  ;

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

      as::socket_base_extra::no_check opt(false);
      socket.set_option(opt, ec);
      if(ec)
      {
        string u = "Cannot set NO_CHECK option: ";
        u += ec.message();
        throw runtime_error(u);
      }

      link_statistic stat(1000, 1000);

      cout << "Listening" << endl;

      nat received = 0;
      vector<char> buf(size);
      packet_receiver rx(log_file);

      while(count == 0 || received < count)
      {
        if(received > 0 && received % 10000 == 0)
        {
          cout << "Received: " << stat << endl;
        }

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
      }
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
