CC=g++

all: server cc

server:
	$(CC) edge/communication/coap-server.cpp -lcoap-3 -lsense -ljsoncpp -o server

cc:
	$(CC) client/communication/coap-client.cpp -lcoap-3 -o coap-client

clean:
	rm server coap-client