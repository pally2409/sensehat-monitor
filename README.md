# sensehat-monitor

IoT application for monitoring environmental data developed using libcoap, I2C and GPIO

## Installing and Compiling
You will need to install the following libraries before compiling:
1. sensehat-driver: https://github.com/mnk400/sensehat-driver
2. jsoncpp: https://github.com/open-source-parsers/jsoncpp
3. libcoap: https://github.com/obgm/libcoap

To build the server and client binaries, run
```
make
```

On separate terminals navigate to the current directory, run
```
./server
```

```
./coap-client
``` 
To change configurations, you can use respective config.json files present in edge and client folder and re-run the executable.

Note: You need to have i2c enabled on the hardware for the compiled software to work, you might also need superuser access.
