#include <iostream>
#include <string>

#include <stdint.h>
#include <argp.h>
#include "generic_argp.h"
#include "tunnel_client_argp.h"

const char *argp_program_version = "TunnelClient 0.1";
const char *argp_program_bug_address = "griff@uio.no";
static char doc[] = "\n"
                    "TunnelClient runs on the inside of a firewall. It connects actively to TunnelServer. "
                    "After that, it provides user-space splicing of a specified TCP connection and turns data arriving the tunnel back into UDP packets.\n"
                    "  <tunnel-rl>\tThe URL, hostname:port or 'dotted decimal address':port of the TunnelServer machine.\n";
static char args_doc[] = "<tunnel-url>";
static struct argp_option options[] = {
    { "fwd-udp",      'u', "string",    0, "(mandatory) The UDP URL of the local machine."},
    { "fwd-tcp",      't', "string",    0, "(mandatory) The TCP URL of the local machine."},
    { 0 }
};

static error_t parse_opt( int key, char *arg, struct argp_state *state )
{
    arguments *args = (arguments*)(state->input);

    switch( key )
    {
    case 'u':
        {
            args->forward_udp_host = arg;
            const int port = extractPort( args->forward_udp_host );
            if( port < 0 )
            {
                std::cerr << "UDP URL does not contain a port" << std::endl;
                return 0;
            }
            args->forward_udp_port = port;
        }
        break;
    case 't':
        {
            args->forward_tcp_host = arg;
            const int port = extractPort( args->forward_tcp_host );
            if( port < 0 )
            {
                std::cerr << "TCP URL does not contain a port" << std::endl;
                return 0;
            }
            args->forward_tcp_port = port;
        }
        break;
    case ARGP_KEY_ARG:
        switch( state->arg_num )
        {
        case 0:
            {
                args->tunnel_host = arg;
                const int port = extractPort( args->tunnel_host );
                if( port < 0 )
                {
                    std::cerr << "URL of the positional command line argument does not contain a port" << std::endl;
                    return 0;
                }
                args->tunnel_port = port;
            }
            break;
        default :
            std::cerr << "Positional command line argument " << state->arg_num << " value " << arg << std::endl;
        }
        break;
    case ARGP_KEY_END:
        if (args->tunnel_host == "")
        {
            argp_error( state, "Positional option <tunnel-host> is missing.");
        }
        if (args->tunnel_port == 0)
        {
            argp_error( state, "Positional option <tunnel-port> is missing.");
        }
        if (args->forward_udp_port == 0)
        {
            argp_error( state, "Mandatory option --fwd-udp (-u) is missing.");
        }
        if (args->forward_tcp_port == 0)
        {
            argp_error( state, "Mandatory option --fwd-tcp (-t) is missing.");
        }
        return 0;
    default:
        return 0;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

void callArgParse( int argc, char* argv[], arguments& args )
{
    argp_parse( &argp, argc, argv, 0, 0, &args );
}

