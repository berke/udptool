// boost_program_options_required_fix.hpp
//
// Author: Berke Durak <berke.durak@gmail.com>

#ifndef BOOST_PROGRAM_OPTIONS_REQUIRED_FIX_HPP_20100525
#define BOOST_PROGRAM_OPTIONS_REQUIRED_FIX_HPP_20100525

#if BOOST_PROGRAM_OPTIONS_VERSION <= 2
  #define bpo_required 
#else
  #define bpo_required ->required()
#endif

#endif
