#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <string>
#include <iostream>
#include <algorithm>
#include <thread>
#include <coap3/coap.h>
#include <jsoncpp/json/json.h>
#include <common/config-util.h>
#include "client/actuator/gpio_pwm.h"

using namespace std;
static unsigned int token = 0;
coap_context_t *ctx = nullptr;

// Default min and max values to map for PWM
int MAX_TEMP = 50;
int MIN_TEMP = -10;
int MAX_HUMID = 0;
int MIN_HUMID = 100;

// Defining atomic int variables so that they can be shared between
// the main tread and the threads running the PWM.
std::atomic<int> temperature_duty;
std::atomic<int> humidity_duty;

class coapClientConnector
{
    public:
    coapClientConnector(const char* host, const char* port)
    {
        // Setting host and port values
        host = host;
        port = port;

        //Starting the coap client
        initClient(host, port, dst);
        coap_startup();
    }
    
    void initClient(const char* host, const char* port, coap_address_t dst)
    {
        auto protocol_type = COAP_PROTO_UDP;
        if (resolve_address(host, port, &dst) < 0)
        {
            coap_log(LOG_CRIT, "Failed to resolve address\n");
        }

        ctx = coap_new_context(nullptr);
        // if(encrypted) {
        //     protocol_type = COAP_PROTO_DTLS  
        // }

        // if(encrypted == 0) {

        // }
        if (!ctx || !(session = coap_new_client_session(ctx, nullptr, &dst, protocol_type)))
        {
            coap_log(LOG_EMERG, "Could not create client session\n");
        }  
    }

    void sendGET(std::string resource, bool confirmable, bool observe)
    {
        coap_optlist_t *optlist_chain = NULL;
        auto message_type = COAP_MESSAGE_CON;
        if(!confirmable) {
          message_type = COAP_MESSAGE_NON;
        }

        std::cout << resource.c_str() << std::endl;

        coap_register_response_handler(ctx, [](auto, auto, const coap_pdu_t *received, auto)
									  {
                                      const unsigned char *data;
                                      size_t data_len;
                                      coap_get_data(received, &data_len, &data);
                                      actuate(std::string(reinterpret_cast<const char*>(data)));
                                      return COAP_RESPONSE_OK;
                                      });

        pdu = coap_pdu_init(message_type, COAP_REQUEST_CODE_GET, coap_new_message_id(session), coap_session_max_pdu_size(session));

        if (!pdu)
		{
          	coap_log( LOG_EMERG, "cannot create PDU\n" );
        }

        token++;

        if (!coap_add_token(pdu, sizeof(token), (unsigned char*)&token))
		{
          	coap_log(LOG_DEBUG, "cannot add token to request\n");
        }

        coap_insert_optlist(&optlist_chain, coap_new_optlist(COAP_OPTION_URI_PATH, resource.size(), reinterpret_cast<const uint8_t *>(resource.c_str())));

        if(observe)
		{   
            std::cout << "in observer" << std::endl;
          	coap_insert_optlist(&optlist_chain, coap_new_optlist(COAP_OPTION_OBSERVE, COAP_OBSERVE_ESTABLISH, NULL));
        }

        coap_add_optlist_pdu(pdu, &optlist_chain);
        coap_show_pdu(LOG_WARNING, pdu);
        coap_send(session, pdu);
        coap_io_process(ctx, COAP_IO_WAIT);
    }

    void connectClient()
	{
      	coap_startup();
    }

    void disconnectClient()
	{
        coap_session_release(session);
        coap_free_context(ctx);
        coap_cleanup();
    }

    int resolve_address(const char *host, const char *service, coap_address_t *dst)
	{
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

		for (ainfo = res; ainfo != NULL; ainfo = ainfo->ai_next)
		{
			switch (ainfo->ai_family)
			{
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

    static void actuate(std::string payload)
    {   
        // Building the json object from the payload
        Json::CharReaderBuilder builder;
        Json::CharReader * reader = builder.newCharReader();
        Json::Value sensor_json;
        string errors;

        reader->parse(payload.c_str(), payload.c_str() + payload.size(), &sensor_json, &errors);
        delete reader;

        float temperature = sensor_json.get("SensorData", 0).get("Temperature", 0).get("Value", 0).asFloat();
        float humidity = sensor_json.get("SensorData", 0).get("Humidity", 0).get("Value", 0).asFloat();
        int pressure = sensor_json.get("SensorData", 0).get("Pressure", 0).get("Value", 0).asInt();

        std::cout << sensor_json << std::endl; 
        // Logic to change Brighten or damped LED brightness based by
        // chaning the PWM duty cycle
        temperature_duty = 100 - ((temperature - MIN_TEMP) * 100 / (MAX_TEMP - MIN_TEMP));
        humidity_duty  = 100 - ((humidity - MIN_HUMID) * 100 / (MAX_HUMID - MIN_HUMID));
        // std::cout << temperature_duty << std::endl;        
    }

    private:
    // Variables for coap communication
	const char* host;
	const char* port;
	coap_session_t *session = nullptr;
	coap_address_t dst;
	coap_pdu_t *pdu = nullptr;
};

int main(void)
{   
    // Defining the default PWM duty cycles.
    temperature_duty = 0;
    humidity_duty = 0;

    configUtil config_util("client/config.json");

    MAX_TEMP = config_util.get_sensor_threshold("temperature", true);
    MIN_TEMP = config_util.get_sensor_threshold("temperature", false);
    MAX_HUMID = config_util.get_sensor_threshold("humidity", true);
    MIN_HUMID = config_util.get_sensor_threshold("temperature", false);

    // Defining the threads to run PWMs
    std::thread temperature_led(set_gpio, config_util.get_string_value("gpio_pins", "temperature"), std::ref(temperature_duty));
    std::thread humidity_led(set_gpio, config_util.get_string_value("gpio_pins", "humidity"), std::ref(humidity_duty));
    coapClientConnector coapClient("localhost", "5683");

    // Get current sensor values
    coapClient.sendGET("SensorData", true, true);
    while (true) { coap_io_process(ctx, COAP_IO_WAIT); }
}