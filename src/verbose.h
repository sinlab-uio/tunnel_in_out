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
#define LOG_INFO if(g_verbose) std::cerr << "[INFO] " << __FILE__ << ":" << __LINE__ << " "

// Only printed when verbose is enabled - for debug messages
#define LOG_DEBUG if(g_verbose) std::cerr << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " "

// For backward compatibility - maps to LOG_DEBUG
#define CERR if(g_verbose) std::cerr << __FILE__ << ":" << __LINE__
