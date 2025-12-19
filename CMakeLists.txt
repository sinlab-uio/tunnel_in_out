cmake_minimum_required(VERSION 3.14)

#
# This tells CMake that your project's name is homeexam-01, and that the programs
# here are written in C.
#
project(TunnelInOut
        VERSION 0.1
        DESCRIPTION "CMake configuration file for TunnelInOut"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE Debug)

include(CheckFunctionExists) # to check for argp in libc

find_path(ARGP_INCLUDE_PATH "argp.h"
          PATHS /usr/include /opt/homebrew/include/
          NO_DEFAULT_PATH)

if (NOT ARGP_INCLUDE_PATH)
    message(FATAL_ERROR "argp.h not found. Please ensure argp-standalone is installed via Homebrew.")
endif()

check_function_exists("argp_parse" ARGP_IN_LIBC)
if(ARGP_IN_LIBC)
		message(STATUS "argp found in libc")
else()
	# Find the argp library
	find_library(ARGP_LIBRARY "argp"
             	PATHS /opt/homebrew/lib
             	NO_DEFAULT_PATH)

	if (NOT ARGP_LIBRARY)
    	message(FATAL_ERROR "libargp.a not found. Please ensure argp-standalone is installed via Homebrew.")
	endif()
endif()

# Add include directory and link library to your target
include_directories(${ARGP_INCLUDE_PATH})

add_library( tunnelNet
	src/sockaddr.cc src/sockaddr.h
	src/udp.cc src/udp.h
	src/tcp.cc src/tcp.h
	src/generic_argp.cc src/generic_argp.h
	)

add_executable( TunnelIn src/tunnel_in.cc
	                 src/tunnel_in_argp.cc src/tunnel_in_argp.h
	                 src/tunnel_in_dispatch.cc src/tunnel_in_dispatch.h )
target_link_libraries( TunnelIn tunnelNet ${ARGP_LIBRARY} )

add_executable( TunnelOut src/tunnel_out.cc
	                 src/tunnel_out_argp.cc src/tunnel_out_argp.h
	                 src/tunnel_out_dispatch.cc src/tunnel_out_dispatch.h
		 	 src/udp_packet.cc src/udp_packet.h
		 	 src/udp_reconstructor.cc src/udp_reconstructor.h)
target_link_libraries( TunnelOut tunnelNet ${ARGP_LIBRARY} )

add_executable( OutsideUDP src/outside_udp_sender.cc
	                 src/outside_udp_argp.cc src/outside_udp_argp.h )
target_link_libraries( OutsideUDP tunnelNet ${ARGP_LIBRARY} )

add_executable( test src/test.cc )
target_link_libraries( test tunnelNet ${ARGP_LIBRARY} )

