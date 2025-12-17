#pragma once

#include <vector>

#include "udp_packet.h"
#include "udp.h"

class UDPReconstructor
{
    /* Byte from the tunnel are stored in this vector */
    std::vector<char>      _bytes_from_tunnel;

    /* The other side of the tunnel sends a 4-byte length value before sending data.
     * _wait_for_length is true while we are waiting for a length field.
     */
    bool                   _wait_for_length { true };

    /* If we are not waiting for a length field, _wait_for_data contains the number
     * of bytes we are waiting for before we can created a UDP packet for forwarding.
     */
    size_t                 _wait_for_data { 0 };

    std::vector<UDPPacket> reconstructed_packets;
    bool                   write_blocked { false };
    bool                   verbose { true };
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

