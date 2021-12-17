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
#include <common/config-util.h>

using namespace std;
static unsigned int token = 0;
coap_context_t *ctx = nullptr;

class coapClientConnector
{
    public:
    coapClientConnector(const char* host, const char* port)
    {
        host = host;
        port = port;
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
                                      coap_show_pdu(LOG_WARNING, received);
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
          	coap_insert_optlist(&optlist_chain, coap_new_optlist(COAP_OPTION_OBSERVE, COAP_OBSERVE_ESTABLISH, NULL));
        }

        coap_add_optlist_pdu(pdu, &optlist_chain);
        coap_show_pdu(LOG_WARNING, pdu);
          /* and send the PDU */
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
    private:
	const char* host;        // Attribute (int variable)
	const char* port;  // Attribute (string variable)
	coap_session_t *session = nullptr;
	coap_address_t dst;
	coap_pdu_t *pdu = nullptr;
};

int main(void)
{
  coapClientConnector coapClient("localhost", "5683");
  coapClient.sendGET("SensorData", false, true);
  coapClient.sendGET("hello", true, true);
  while (true) { coap_io_process(ctx, COAP_IO_WAIT); }
}