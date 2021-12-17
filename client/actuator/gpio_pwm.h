#include <atomic>
#include <string>

#define PWM_PERIOD 10000

void set_gpio(std::string gpio, std::atomic<int> &duty_cycle);