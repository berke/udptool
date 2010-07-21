// udpflood
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

int main(int argc, char* argv[]) //{{{
{
  typedef const char *option;
  
  string s_ip = "0.0.0.0";
  nat s_port = 5000;
  string d_ip;
  nat d_port = 0;
  nat count = 0;
  bool verbose = false;

  po::options_description desc("Available options");
  desc.add_options()
    ("help,h",                                                   "Display this information")
    ("sip",           po::value<string>(&s_ip),                  "Source IP to bind to")
    ("sport",         po::value<nat>(&s_port) bpo_required,       "Source port to bind to")
    ("dip",           po::value<string>(&d_ip) bpo_required,      "Destination IP to transmit to")
    ("dport",         po::value<nat>(&d_port) bpo_required,       "Destination port to transmit to")
    ("size",          po::value< vector<size_t> >() bpo_required, "Add a packet size")
    ("delay",         po::value< vector<double> >() bpo_required, "Add a packet transmission delay (ms)")
    ("count",         po::value<nat>(&count),                    "Number of packets to send, or 0 for no limit)")
    ("verbose",       po::bool_switch(&verbose),                 "Display each packet as it is sent")
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
      // Resolve destination address
      cout << "Resolving " << d_ip << " port " << d_port << endl;
      udp::resolver resolver(io);
      udp::resolver::query query(udp::v4(), d_ip, to_string(d_port));
      udp::endpoint receiver_endpoint = *resolver.resolve(query);

      cout << "Opening socket" << endl;
      udp::endpoint src(as::ip::address::from_string(s_ip), s_port);
      udp::socket socket(io, src);

      nat sent = 0;
      microsecond_timer::microseconds t0 = microsecond_timer::get();
      size_t bytes = 0;

      cout << "Starting flood" << endl;
      boost::asio::deadline_timer t(io);

      vector<size_t> sizes (vm["size" ].as< vector<size_t> >());
      vector<double> delays(vm["delay"].as< vector<double> >());

      vector<double>::iterator d_it = delays.begin();
      vector<size_t>::iterator s_it = sizes.begin();

      while(count == 0 || sent < count)
      {
        if(sent > 0 && sent % 100 == 0)
        {
          microsecond_timer::microseconds t = microsecond_timer::get();
          double dt = (t - t0) / 1e6;
          double throughput = bytes / dt;
          cout << "Processed " << sent << " packets; throughput " << throughput/1e6 << " MB/s" << endl;
        }
        sent ++;

        if(d_it == delays.end()) d_it = delays.begin();
        double delay = *d_it ++;

        if(s_it == sizes.end()) s_it = sizes.begin();
        size_t size = *s_it ++;

        std::vector<char> buf(size);
        for(nat i = 0; i < size; i ++)
        {
          buf[i] = random() & 255;
        }

        if(delay > 0) t.expires_from_now(boost::posix_time::microseconds(delay * 1e3));
        socket.send_to(boost::asio::buffer(buf), receiver_endpoint);
        if(verbose) cerr << size << " " << delay << endl;
        bytes += size;
        if(delay > 0) t.wait();
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
