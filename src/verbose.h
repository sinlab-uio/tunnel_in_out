#pragma once

#include <iostream>

// Global verbosity flag
// Can be set via command line arguments
extern bool g_verbose;

// Macros for different message types

// Always printed - for errors
#define LOG_ERROR std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__ << " "

// Always printed - for warnings
#define LOG_WARN std::cerr << "[WARN] " << __FILE__ << ":" << __LINE__ << " "

// Only printed when verbose is enabled - for informational messages
#define LOG_INFO if(__builtin_expect(g_verbose, 0)) std::cerr << "[INFO] " << __FILE__ << ":" << __LINE__ << " "

// Only printed when verbose is enabled - for debug messages
#define LOG_DEBUG if(__builtin_expect(g_verbose, 0)) std::cerr << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " "

