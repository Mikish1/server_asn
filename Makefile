
.PHONY: clean

CFLAGS= -Wall

TCPserver: TCPserver.o
	gcc $(CFLAGS) -o TCPserver TCPserver.o

TCPclient: TCPclient.o
	gcc $(CFLAGS) -o TCPclient TCPclient.o

TCPproxy: TCPproxy.o
	gcc $(CFLAGS) -o TCPproxy TCPproxy.o

UDPserver: UDPserver.o
	gcc $(CFLAGS) -o UDPserver UDPserver.o

UDPproxy: UDPproxy.o
	gcc $(CFLAGS) -o UDPproxy UDPproxy.o

clean:
	rm -f *.o all