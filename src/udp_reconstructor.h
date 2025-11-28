#pragma once

#include <vector>

#include "udp_packet.h"
#include "udp.h"

class UDPReconstructor
{
    bool                   wait_for_length { true };
    std::vector<char>      bytes_from_tunnel;
    std::vector<UDPPacket> reconstructed_packets;
    bool                   write_blocked { false };
    bool                   verbose { false };
public:
    void setNoBlock( UDPSocket& write_socket );

    void collect_from_tunnel( const char* buffer, int buflen );

    void sendLoop( UDPSocket& udp_forwarder, const SockAddr& dest );

    inline bool isWriteBlocked() const { return write_blocked; }
    inline void unblockWrite() { write_blocked = false; }

    inline bool empty() const { return reconstructed_packets.empty(); }

    inline const UDPPacket& front() const { return reconstructed_packets.front(); }

    inline void setVerbose( bool onoff ) { verbose = onoff; }
};

