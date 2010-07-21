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
  nat count = 0;
  size_t size;

  po::options_description desc("Available options");
  desc.add_options()
    ("help,h",                                                   "Display this information")
    ("sip",           po::value<string>(&s_ip),                  "Source IP to bind to")
    ("sport",         po::value<nat>(&s_port) bpo_required,       "Source port to bind to")
    ("count",         po::value<nat>(&count),                    "Number of packets to receive, or 0 for no limit)")
    ("size",          po::value<size_t>(&size),                  "Reception buffer size")
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
      cout << "Opening socket" << endl;
      udp::endpoint src(as::ip::address::from_string(s_ip), s_port);
      udp::socket socket(io, src);

      link_statistic stat(1000, 1000);

      cout << "Listening" << endl;

      nat received = 0;
      std::vector<char> buf(size);

      while(count == 0 || received < count)
      {
        if(received > 0 && received % 100 == 0)
        {
          cout << "Received: " << stat << endl;
        }
        received ++;

        boost::system::error_code ec;
        udp::endpoint remote;
        size_t received = socket.receive_from(boost::asio::buffer(buf), remote, 0, ec);
        if(ec)
        {
          cout << "Reception error: " << ec.message() << endl;
        }
        else
        {
          stat.add(received);
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
