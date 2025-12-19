#pragma once

#include <string>

/* Take in a URL as parameter. Split off the tail end that supposedly
 * contains a port number. Return the port number as an int, and keep
 * the hostname in the parameter url.
 * This does not work with IPv6 addresses, which contains colons.
 */
int extractPort( std::string& url );

