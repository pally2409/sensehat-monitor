CC=g++

all: server cc

server:
	$(CC) edge/communication/coap-server.cpp common/config-util.cpp common/connection-helper.c -lcoap-3 -lsense -ljsoncpp -o server -I $(shell pwd)

cc:
	$(CC) client/communication/coap-client.cpp client/actuator/gpio_pwm.cpp common/config-util.cpp common/connection-helper.c -lcoap-3 -ljsoncpp -lpthread -o coap-client -I $(shell pwd)

clean:
	rm server coap-client