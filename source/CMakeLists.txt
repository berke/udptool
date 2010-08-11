enable_language(CXX)
set(CMAKE_CXX_FLAGS "-Wall -std=c++0x")

message("Boost: includes ${BOOST_INCLUDES}, libs ${BOOST_LIBS}")
include_directories( ${BOOST_INCLUDES} ${include_directories} )
link_directories( ${BOOST_LIBS} ) # ${link_directories} )

add_executable(udptool udptool.cpp microsecond_timer.cpp link_statistic.cpp)
target_link_libraries(udptool boost_program_options boost_system)

add_executable(curx_test curx_test.c curx.c)
target_link_libraries(curx_test)
