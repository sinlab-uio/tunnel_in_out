#include <iostream>
#include <string>

#include <argp.h>

int extractPort( std::string& url )
{
    const size_t it = url.find( ":" );
    if( it == std::string::npos ) return -1;

    const int port = std::stoi( url.substr( it+1 ) );
    url.erase( it );
    return port;
}

