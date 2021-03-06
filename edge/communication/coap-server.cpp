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
#include <sense_hat.h>
#include <common/config-util.h>
#include <common/connection-helper.h>
#include <unistd.h>

using namespace std;
coap_resource_t *sensor_resource = NULL;

// File path for certificates required during encryption
char *certificate;
char *certificate_authority;
char *private_key;

class coapServerGateway
{
	public:
    coapServerGateway(const char* host, const char* port, bool observable, bool encrypted, int poll_rate)
    {
        // Setting host and port values
        this->host = host;
        this->port = port;

        // Setting encrypted, observable, poll_rate
        this->observable = observable;
        this->encrypted = encrypted;
        this->poll_rate = poll_rate;

        std::cout << "Host: " << host << "\nPort: " << port << "\nObservable: " << (observable? "true": "false") << "\nEncrypted: " << (encrypted? "true": "false") << std::endl;
        
        // Initializing server parameters
        initServer(host, port, dst);
        
    }

    /**
    * Initializes the coap server
    *
    * @param host host to use for our endpoint
    * @param port port for our endpoint
    * @param dst represents the resolved address destination we use for endpoints
    */
    void initServer(const char* host, const char* port, coap_address_t dst)
    { 
        // Resolve address to create creation and check if successful
        if (resolve_address(host, port, &dst) < 0)
        {
            coap_log(LOG_CRIT, "Failed to resolve address\n");
        }

        // Create a new context to represent our connection
        ctx = coap_new_context(nullptr);

        // Default protocol is UDP, if encrypted set, use DTLS. Load certs if encrypted is sets
        auto protocol = COAP_PROTO_UDP;
        if(this->encrypted == true) {
            set_certificate(certificate);
            set_certificate_authority(certificate_authority);
            set_private_key(private_key);
            fill_keystore(ctx);
            protocol = COAP_PROTO_DTLS;
            std::cout << "Loaded certs ..." << "\nCA: " << certificate_authority << "\nPrivate Key: " << private_key << "\nCertificate: " << certificate << std::endl;
        }

        // Create the endpoint
        if (!ctx || !(endpoint = coap_new_endpoint(ctx, &dst, protocol)))
        {
            coap_log(LOG_EMERG, "Could not initialize context\n");
        }
    }

    /**
    * Start the CoAP server
    */
    void startServer()
    {
        coap_startup();
        while (true) { 
            coap_io_process(ctx, poll_rate * 1000);
            if(sensor_resource!=NULL && observable == true) {
                // Use the poll rate to determine how often to send the data
                // Notify observers if observable is set to true
                coap_resource_notify_observers(sensor_resource, NULL);
            } 
        }
    }

    /**
    * Stop the CoAP server and clean up
    */
    void stopServer() {
        coap_free_context(ctx);
        coap_cleanup();
    }

    /**
    * Register a new resource
    *
    * @param resourceName resource to register
    */
    void registerResource(const char* resourceName)
    {
        coap_str_const_t *ruri = coap_make_str_const(resourceName);
        resource = coap_resource_init(ruri, 0);

        // Set sensor_resource (we use this to send new data to observers if observable is set)
        if(strcmp(resourceName, "SensorData") == 0) {
            sensor_resource = resource;
        }

        // Make resource observable
        if(observable == true) {
            coap_resource_set_get_observable(resource, 1);
        }
        std::cout << "Registering Resource: " << resourceName << std::endl;
        coap_register_handler(resource, COAP_REQUEST_GET, [](auto, auto, const coap_pdu_t *request, auto, 
                            coap_pdu_t *response)
                            {
                                coap_show_pdu(LOG_WARNING, request);
                                coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
                                auto str = coapServerGateway::getSensorData();
                                coap_add_data(response, str.size(), reinterpret_cast<const uint8_t *>(str.c_str()));
                                coap_show_pdu(LOG_WARNING, response);
                            });                
        coap_add_resource(ctx, resource);
        
    }

    /**
    * Use the sensehat-driver to get the latest sensor data
    *
    * @return String containing json payload with sensor data
    */
    static std::string getSensorData()
    {
        // Define json params
        Json::Value root;
        Json::Value data;
        Json::Value pressure;
        Json::Value temp;
        Json::Value humidity;
        if (init(1) == 0)
        {
            printf("Unable to initialize sense_hat");
        }
        else
        {   
            // Use sensehat-driver to get latest sensor values
            temp["Value"] = get_temperature();
            pressure["Value"] = get_pressure();
            humidity["Value"] = get_humidity();
            data["Temperature"] = temp;
            data["Pressure"] = pressure;
            data["Humidity"] = humidity;
            root["SensorData"] = data;
            stop();
        }
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        const std::string json_file = Json::writeString(builder, root);
        return json_file;
    }

    private:
    const char* host;   
    const char* port;
    int poll_rate;
    bool observable;
    bool encrypted;
    coap_context_t  *ctx = nullptr;
    coap_address_t dst;
    coap_resource_t *resource = nullptr;
    coap_endpoint_t *endpoint = nullptr;
};

int main(void)
{
    configUtil cUtil("edge/config.json");

    // Determine encryption, observable from configuration
    bool encrypted = cUtil.get_bool_value("connection", "encrypt");
    bool observable = cUtil.get_bool_value("connection", "observable");
    bool poll_rate = cUtil.get_bool_value("data", "rate");
    const char* port;

    // If encryped, set values for cert path and set port to the secure coap port, else set port to the default coap port
    if(encrypted) {
        port = cUtil.get_string_value("connection", "secure_port").c_str();
        std::string str_cert = cUtil.get_string_value("connection", "cert_path");
        certificate = new char[str_cert.length() + 1];
        strcpy(certificate, str_cert.c_str());
        std::string str_ca = cUtil.get_string_value("connection", "ca_path");
        certificate_authority = new char[str_ca.length() + 1];
        strcpy(certificate_authority, str_ca.c_str());
        std::string str_pk = cUtil.get_string_value("connection", "key_path");
        private_key = new char[str_pk.length() + 1];
        strcpy(private_key, str_pk.c_str());
    } else {
        port = cUtil.get_string_value("connection", "default_port").c_str();
    }

    // Initialize and start the server
    coapServerGateway coapServer(cUtil.get_string_value("connection", "host").c_str(), port, observable, encrypted, poll_rate);
    coapServer.registerResource(cUtil.get_string_value("data", "sensor_resource").c_str());
    coapServer.startServer();
}