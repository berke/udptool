// no_check_socket_optin.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#ifndef NO_CHECK_SOCKET_OPTION_HPP_20100722
#define NO_CHECK_SOCKET_OPTION_HPP_20100722

#ifdef __LINUX__

  #define HAVE_SO_NO_CHECK 1

  namespace boost
  {
    namespace asio
    {
      namespace socket_base_extra
      {
        typedef boost::asio::detail::socket_option::boolean<SOL_SOCKET, SO_NO_CHECK> no_check;
      };
    };
  };

#else

  #define HAVE_SO_NO_CHECK 0

#endif

#endif

