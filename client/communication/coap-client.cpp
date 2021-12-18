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
#include <common/connection-helper.h>

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

// File path for certificates required during encryption
char *certificate;
char *certificate_authority;
char *private_key;

class coapClientConnector
{
    public:
    coapClientConnector(const char* host, const char* port, bool observable, bool encrypted, bool confirmable, const char* sensor_resource)
    {
        // Setting host and port values
        this->host = host;
        this->port = port;

        // Setting encrypted, observable, confirmable and sensor resource
        this->observable = observable;
        this->encrypted = encrypted;
        this->confirmable = confirmable;
        this->sensor_resource = sensor_resource;

        std::cout << "Host: " << host << "\nPort: " << port << "\nObservable: " << (observable? "true": "false") << "\nEncrypted: " << (encrypted? "true": "false") << "\nConfirmable: " << (confirmable? "true": "false") << std::endl;

        // Initializing client parameters
        initClient(this->host, this->port, dst);

        // Start the client
        coap_startup();
    }
    
    /**
    * Initializes the coap client
    *
    * @param host host to which we want to connect to.
    * @param port to which we want to connect to
    * @param dst represents the resolved address destination we use for endpoints
    */
    void initClient(const char* host, const char* port, coap_address_t dst)
    {
        // Resolve address to create creation and check if successful
        if (resolve_address(host, port, &dst) < 0)
        {
            coap_log(LOG_CRIT, "Failed to resolve address\n");
        }

        // Create a new context to represent our connection
        ctx = coap_new_context(nullptr);

        // Default protocol is UDP, if encrypted set, use DTLS. Set DTLS role client if encrypted is set and start the session
        if(encrypted == true) {
            std::cout << "Loaded certs ..." << "\nCA: " << certificate_authority << "\nPrivate Key: " << private_key << "\nCertificate: " << certificate << std::endl;
            set_certificate(certificate);
            set_certificate_authority(certificate_authority);
            set_private_key(private_key);
            coap_dtls_pki_t *dtls_pki = setup_pki(COAP_DTLS_ROLE_CLIENT);
            session = coap_new_client_session_pki(ctx, nullptr, &dst, COAP_PROTO_DTLS, dtls_pki);
        }
        else {  
            session = coap_new_client_session(ctx, nullptr, &dst, COAP_PROTO_UDP);
        }

        // Check if session creation was successful
        if (!ctx || !(session))            
        {
            coap_log(LOG_EMERG, "Could not create client session\n");
        }  
    }

    /**
    * Sends a GET request to the CoAP server
    *
    * @param resource resource at the server we want to send the request to
    */
    void sendGET(std::string resource)
    {
        coap_optlist_t *optlist_chain = NULL;

        // Default message type is confirmable, if confirmable set to false, change message to Non-confirmable
        auto message_type = COAP_MESSAGE_CON;
        if(confirmable == false) {
          message_type = COAP_MESSAGE_NON;
        }

        std::cout << "Sending GET request for resource: " << resource << std::endl;

        // If response was for sensor, we attach process the data and trigger actuation event accordingly
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

        // Use a token to identify a request
        token++;

        if (!coap_add_token(pdu, sizeof(token), (unsigned char*)&token))
		{
          	coap_log(LOG_DEBUG, "cannot add token to request\n");
        }

        // Set generic options
        coap_insert_optlist(&optlist_chain, coap_new_optlist(COAP_OPTION_URI_PATH, resource.size(), reinterpret_cast<const uint8_t *>(resource.c_str())));


        // Set observable specific options
        if(observable == true)
		{   
            std::cout << "Setting resource: " << resource << " as observable" << std::endl;
          	coap_insert_optlist(&optlist_chain, coap_new_optlist(COAP_OPTION_OBSERVE, COAP_OBSERVE_ESTABLISH, NULL));
        }

        coap_add_optlist_pdu(pdu, &optlist_chain);
        coap_show_pdu(LOG_WARNING, pdu);
        coap_send(session, pdu);
        coap_io_process(ctx, COAP_IO_WAIT);
    }

    /**
    * Disconnect and clean up 
    */
    void disconnectClient()
	{
        coap_session_release(session);
        coap_free_context(ctx);
        coap_cleanup();
    }

    /**
    * Process incoming sensor data and perform appropriate actuation
    * @param payload incoming sensor data payload
    */
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
    const char* sensor_resource;
    bool observable;
    bool encrypted;
    bool confirmable;
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

    // Determine thresholds for humidity and temperature sensors
    MAX_TEMP = config_util.get_sensor_threshold("temperature", true);
    MIN_TEMP = config_util.get_sensor_threshold("temperature", false);
    MAX_HUMID = config_util.get_sensor_threshold("humidity", true);
    MIN_HUMID = config_util.get_sensor_threshold("temperature", false);

    // Defining the threads to run PWMs
    std::thread temperature_led(set_gpio, config_util.get_string_value("gpio_pins", "temperature"), std::ref(temperature_duty));
    std::thread humidity_led(set_gpio, config_util.get_string_value("gpio_pins", "humidity"), std::ref(humidity_duty));

    // Determine encryption, observable and confirmable from configuration
    bool encrypted = config_util.get_bool_value("connection", "encrypt");
    bool observable = config_util.get_bool_value("connection", "observable");
    bool confirmable = config_util.get_bool_value("connection", "confirmable");
    const char* sensor_resource = config_util.get_string_value("data", "sensor_resource").c_str();
    const char* port;

    // If encryped, set values for cert path and set port to the secure coap port, else set port to the default coap port
    if(encrypted == true) {
        port = config_util.get_string_value("connection", "secure_port").c_str();
        std::string str_cert = config_util.get_string_value("connection", "cert_path");
        certificate = new char[str_cert.length() + 1];
        strcpy(certificate, str_cert.c_str());
        std::string str_ca = config_util.get_string_value("connection", "ca_path");
        certificate_authority = new char[str_ca.length() + 1];
        strcpy(certificate_authority, str_ca.c_str());
        std::string str_pk = config_util.get_string_value("connection", "key_path");
        private_key = new char[str_pk.length() + 1];
        strcpy(private_key, str_pk.c_str());
    } else {
        port = config_util.get_string_value("connection", "default_port").c_str();
    }
    coapClientConnector coapClient(config_util.get_string_value("connection", "host").c_str(), port, observable, encrypted, confirmable, sensor_resource);

    // Get current sensor values
    coapClient.sendGET(sensor_resource);
    while (true) { coap_io_process(ctx, COAP_IO_WAIT); }
}