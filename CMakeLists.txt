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

add_subdirectory(src)

