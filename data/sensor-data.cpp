#include<iostream>

using namespace std;

class sensorData {       // The class
  public:             // Access specifier
    const char* value;        // Attribute (int variable)
    const char* type;  // Attribute (string variable)
    sensorData(const char* type, const char* value) {     // Constructor
      type = type;
      value = value;
    }
};