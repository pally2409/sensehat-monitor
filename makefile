CC=g++

all: server cc

server:
	$(CC) edge/communication/coap-server.cpp -lcoap-3 -lsense -ljsoncpp -o server

cc:
	$(CC) client/communication/coap-client.cpp client/actuator/gpio_pwm.cpp -lcoap-3 -ljsoncpp -lpthread -o coap-client -I $(shell pwd)

clean:
	rm server coap-client