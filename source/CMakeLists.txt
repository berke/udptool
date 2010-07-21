enable_language(CXX)
set(CMAKE_CXX_FLAGS "-Wall -std=c++0x")

message("Boost: includes ${BOOST_INCLUDES}, libs ${BOOST_LIBS}")
include_directories( ${BOOST_INCLUDES} ${include_directories} )
link_directories( ${BOOST_LIBS} ) # ${link_directories} )

add_executable(udpflood udpflood.cpp microsecond_timer.cpp)
target_link_libraries(udpflood boost_program_options boost_system)

add_executable(udprecv udprecv.cpp microsecond_timer.cpp link_statistic.cpp)
target_link_libraries(udprecv boost_program_options boost_system)
