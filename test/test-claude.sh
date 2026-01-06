# Should work exactly like before, but with new protocol
./TunnelServer --tunnel-tcp 8888 --outside-udp 9999 --outside-tcp 7777
./TunnelClient --tunnel-host localhost --tunnel-port 8888 \
               --forward-udp-host localhost --forward-udp-port 5555
echo "test" | nc -u localhost 9999

