#include <jsoncpp/json/json.h>
using namespace std;

class configUtil
{
    public:       
        Json::Value root;
        
        configUtil(const char* file_name);
        
        std::string get_string_value(const char* section, const char* key);
        
        int get_sensor_threshold(const char* sensor_name, bool lo);
        
        bool get_bool_value(const char* section, const char* key);

};