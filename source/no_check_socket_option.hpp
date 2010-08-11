// no_check_socket_optin.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>
// vim:set ts=2 sw=2 foldmarker={,}:

#ifndef NO_CHECK_SOCKET_OPTION_HPP_20100722
#define NO_CHECK_SOCKET_OPTION_HPP_20100722

#ifdef __linux__

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

