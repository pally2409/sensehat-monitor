#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <coap3/coap.h>
#include <iostream>
#include <jsoncpp/json/json.h>

using namespace std;
coap_resource_t *sensor_resource = NULL;

class coapServerGateway {
  public:             // Access specifier
    coapServerGateway(const char* host, const char* port) {     // Constructor
      host = host;
      port = port;
      initServer(host, port, dst);
    }
    void initServer(const char* host, const char* port, coap_address_t dst) {
      if (resolve_address(host, port, &dst) < 0) {
        coap_log(LOG_CRIT, "Failed to resolve address\n");
        // goto finish;
      }

      ctx = coap_new_context(nullptr);

      if (!ctx || !(endpoint = coap_new_endpoint(ctx, &dst, COAP_PROTO_UDP))) {
        coap_log(LOG_EMERG, "cannot initialize context\n");
        // goto finish;
      }
      
    }

    void startServer() {
      coap_startup();
      while (true) { coap_io_process(ctx, COAP_IO_WAIT); }
    }

    void stopServer() {
      coap_free_context(ctx);
      coap_cleanup();
    }

    void registerResource(const char* resourceName) {
      coap_str_const_t *ruri = coap_make_str_const(resourceName);
      resource = coap_resource_init(ruri, 0);
      coap_resource_set_get_observable(resource, 1);
      coap_register_handler(resource, COAP_REQUEST_GET, [](auto, auto, const coap_pdu_t *request, auto, 
                          coap_pdu_t *response) {
                          coap_show_pdu(LOG_WARNING, request);
                          coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
                          auto str = coapServerGateway::getSensorData();
                          coap_add_data(response, str.size(), reinterpret_cast<const uint8_t *>(str.c_str()));
                          coap_show_pdu(LOG_WARNING, response);
                          
                        });
      coap_register_handler(resource, COAP_REQUEST_PUT, [](coap_resource_t *curr_resource, auto, const coap_pdu_t *request, auto, 
                          coap_pdu_t *response) {
                          coap_show_pdu(LOG_WARNING, request);
                          coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
                          auto str = coapServerGateway::getSensorData();
                          coap_add_data(response, str.size(), reinterpret_cast<const uint8_t *>(str.c_str()));
                          coap_show_pdu(LOG_WARNING, response);
                          coap_str_const_t *uri_path = coap_resource_get_uri_path(curr_resource);
                          if(coap_string_equal(uri_path, coap_make_str_const("SensorData"))){
                            std::cout << "here";
                            coap_resource_notify_observers(sensor_resource, NULL);
                          }
                        });                  
      coap_add_resource(ctx, resource);
      if(std::strcmp(resourceName, "SensorData")) {
        sensor_resource = resource;
      }
    }

    static std::string getSensorData() {
      // sensorData pressureData("pressure", "1020"); 
      // sensorData tempData("temp", "27"); 
      // sensorData humidityData("humidity", "30"); 
      Json::Value root;
      Json::Value data;
      Json::Value pressure;
      Json::Value temp;
      Json::Value humidity;
      temp["Value"] = 37;
      pressure["Value"] = 1020;
      humidity["Value"] = 30;
      data["Temperature"] = temp;
      data["Pressure"] = pressure;
      data["Humidity"] = humidity;
      root["SensorData"] = data;
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      const std::string json_file = Json::writeString(builder, root);
      return json_file;
    }

    int resolve_address(const char *host, const char *service, coap_address_t *dst) {

      struct addrinfo *res, *ainfo;
      struct addrinfo hints;
      int error, len=-1;

      memset(&hints, 0, sizeof(hints));
      memset(dst, 0, sizeof(*dst));
      hints.ai_socktype = SOCK_DGRAM;
      hints.ai_family = AF_UNSPEC;

      error = getaddrinfo(host, service, &hints, &res);

      if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return error;
      }

      for (ainfo = res; ainfo != NULL; ainfo = ainfo->ai_next) {
        switch (ainfo->ai_family) {
        case AF_INET6:
        case AF_INET:
          len = dst->size = ainfo->ai_addrlen;
          memcpy(&dst->addr.sin6, ainfo->ai_addr, dst->size);
          goto finish;
        default:
          ;
        }
      }

      finish:
      freeaddrinfo(res);
      return len;
}
    private:
      const char* host;        // Attribute (int variable)
      const char* port;  // Attribute (string variable)
      coap_context_t  *ctx = nullptr;
      coap_address_t dst;
      coap_resource_t *resource = nullptr;
      coap_endpoint_t *endpoint = nullptr;
};

int main(void) {
  coapServerGateway coapServer("localhost", "5683");
  coapServer.registerResource("SensorData");
  coapServer.registerResource("hello");
  std::cout << coapServer.getSensorData().c_str() << std::endl;

  coapServer.startServer();
  
}