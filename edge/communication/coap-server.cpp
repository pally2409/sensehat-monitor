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
char *certificate;
char *certificate_authority;
char *private_key;

class coapServerGateway
{
	public:
    coapServerGateway(const char* host, const char* port, bool observable, bool encrypted)
    {
        host = host;
        port = port;
        observable = observable;
        encrypted = encrypted;

        std::cout << "Host: " << host << "\nPort: " << port << "\nObservable: " << (observable? "true": "false") << "\nEncrypted: " << (encrypted? "true": "false") << std::endl;
        initServer(host, port, dst);
        
    }

    void initServer(const char* host, const char* port, coap_address_t dst)
    { 
        if (resolve_address(host, port, &dst) < 0)
        {
            coap_log(LOG_CRIT, "Failed to resolve address\n");
        }

        ctx = coap_new_context(nullptr);
        auto protocol = COAP_PROTO_UDP;
        std::cout << encrypted << std::endl;
        if(encrypted) {
            std::cout << "here" << std::endl;
            set_certificate(certificate);
            set_certificate_authority(certificate_authority);
            set_private_key(private_key);
            fill_keystore(ctx);
            protocol = COAP_PROTO_DTLS;
            std::cout << "Loaded certs ..." << "\nCA: " << certificate_authority << "\nPrivate Key: " << private_key << "\nCertificate: " << certificate << std::endl;
        }
        if (!ctx || !(endpoint = coap_new_endpoint(ctx, &dst, COAP_PROTO_UDP)))
        {
            coap_log(LOG_EMERG, "Could not initialize context\n");
        }
    }

    void startServer()
    {
        coap_startup();
        while (true) { 
            if(sensor_resource!=NULL) {
                usleep(2000000);
                if(observable) {
                    coap_resource_notify_observers(sensor_resource, NULL);
                } 
            }
            coap_io_process(ctx, 5); 
        }
    }

    void stopServer() {
        coap_free_context(ctx);
        coap_cleanup();
    }

    void registerResource(const char* resourceName)
    {
        coap_str_const_t *ruri = coap_make_str_const(resourceName);
        resource = coap_resource_init(ruri, 0);
        if(strcmp(resourceName, "SensorData") == 0) {
            sensor_resource = resource;
        }
        if(observable) {
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

    static std::string getSensorData()
    {
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

        if (error != 0)
        {
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
    private:
    const char* host;   
    const char* port;
    bool* observable;
    bool* encrypted;
    coap_context_t  *ctx = nullptr;
    coap_address_t dst;
    coap_resource_t *resource = nullptr;
    coap_endpoint_t *endpoint = nullptr;
};

int main(void)
{
    configUtil cUtil("edge/config.json");
    bool encrypted = cUtil.get_bool_value("connection", "encrypt") == 0? true : false;
    bool observable = cUtil.get_bool_value("connection", "observable") == 0? true : false;
    const char* port;
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

    coapServerGateway coapServer(cUtil.get_string_value("connection", "host").c_str(), port, observable, encrypted);
    coapServer.registerResource(cUtil.get_string_value("data", "sensor_resource").c_str());
    coapServer.startServer();
}