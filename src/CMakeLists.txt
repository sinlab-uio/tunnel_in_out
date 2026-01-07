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

# ==============================================================================
# GNUInstallDirs CMake module
# - Define GNU standard installation directories
# - Provides install directory variables as defined by the GNU Coding Standards.
# ==============================================================================
include(GNUInstallDirs)

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
	verbose.cc verbose.h
	sockaddr.cc sockaddr.h
	udp.cc udp.h
	tcp.cc tcp.h
	generic_argp.cc generic_argp.h
	tunnel_protocol.cc tunnel_protocol.h
	tunnel_message_reconstructor.cc tunnel_message_reconstructor.h
	tcp_connection_manager.cc tcp_connection_manager.h
	udp_packet.cc udp_packet.h
	)

add_executable( TunnelServer tunnel_server.cc
	                 tunnel_server_argp.cc tunnel_server_argp.h
	                 tunnel_server_dispatch.cc tunnel_server_dispatch.h )
target_link_libraries( TunnelServer tunnelNet ${ARGP_LIBRARY} )

add_executable( TunnelClient tunnel_client.cc
	                 tunnel_client_argp.cc tunnel_client_argp.h
	                 tunnel_client_dispatch.cc tunnel_client_dispatch.h )
target_link_libraries( TunnelClient tunnelNet ${ARGP_LIBRARY} )

add_executable( OutsideUDP outside_udp_sender.cc
	                 outside_udp_argp.cc outside_udp_argp.h )
target_link_libraries( OutsideUDP tunnelNet ${ARGP_LIBRARY} )

add_executable( test test.cc )
target_link_libraries( test tunnelNet ${ARGP_LIBRARY} )

install(TARGETS TunnelServer TunnelClient OutsideUDP )

######################################
# SUMMARY
######################################
message("\n")
message("******************************************")
message("Building configuration:\n")
message(STATUS "Tunnel version: " ${PROJECT_VERSION})
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
message(STATUS "Build Shared libs: " ${BUILD_SHARED_LIBS})
message(STATUS "Generate position independent code: " ${CMAKE_POSITION_INDEPENDENT_CODE})
message(STATUS "Install path: " ${CMAKE_INSTALL_PREFIX})

