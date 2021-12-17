#include <string>
#include <iostream>
#include <fstream>
#include <jsoncpp/json/json.h>

using namespace std;

class configUtil {       // The class
  public:             // Access specifier
    Json::Value root;        // Attribute (int variable)
    configUtil(const char* file_name) {     // Constructor
        std::ifstream ifs;
        ifs.open(file_name);
        Json::CharReaderBuilder builder;
        builder["collectComments"] = true;
        JSONCPP_STRING errs;
        if (!parseFromStream(builder, ifs, &root, &errs)) {
            std::cout << errs << std::endl;
        }
    }

    std::string get_string_value(const char* section, const char* key) {
        auto sec = root[section];
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        const std::string val = Json::writeString(builder, sec[key]);
        return val;
    }

    int get_sensor_threshold(const char* sensor_name, bool lo) {
        auto sec = root["data"];
        auto sensor = sec[sensor_name]["threshold"];
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        int val;
        if(lo) {
            val = sensor["lo"].asInt();
        } else{
            val = sensor["hi"].asInt();
        }
        return val;
    }

    bool get_bool_value(const char* section, const char* key) {
        auto sec = root[section];
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        return sec[key].asBool();
    }
};


