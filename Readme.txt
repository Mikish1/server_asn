
TCPserver
for compiling: make TCPserver
Takes one argument, the port number to connect to
ex) TCPserver 39000


TCPclient
for compiling: make TCPclient
Takes two arguments, first is the host name and second is the port number of the server or proxy server to connect to.
The server, and proxy server when using it, must be started for before running the client.
Commands can be inputed as specified by the requirements.
ex) TCPclient tux6 39000


TCPproxy
for compiling: make TCPproxy
Takes two arguments, first is the hostname and second is the port number the Tcp server and TCP client will be connecting to.
To run, the TCP server should be started first, then the proxy server, and the client last.
ex) TCPproxy tux6 39000


UDPserver
for compiling: make UDPserver
Takes one argument, which is the port number it will connect to with the proxy.
Must be started before the UDP proxy server.
ex) UDPserver 39000


UDPproxy
for compiling: make UDPproxy
Takes two arguments, first is the hostname and second is the port number the UDP server and TCP client will be connecting to.
The UDP server must be started first, then the proxy, and the client last.
ex) UDPproxy tux6 39000