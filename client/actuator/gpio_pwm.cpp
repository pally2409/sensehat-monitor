#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <atomic>
#include "gpio_pwm.h"

void set_gpio(std::string gpio, std::atomic<int> &duty_cycle)
{
    // Accessing the GPIO pins using /sys/class/gpio driver.
    // Selecting pin gpio.
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    write(fd, gpio.c_str(), 2);
    close(fd);
    // Setting pin gpio as output.
    std::string direction = std::string() + "/sys/class/gpio/gpio" + gpio + "/direction";
    fd = open(direction.c_str(), O_WRONLY);
    write(fd, "out", 3);
    close(fd);
    // Opening file using which we will set the value of pin gpio.
    std::string value = std::string() + "/sys/class/gpio/gpio" + gpio + "/value";
    fd = open(value.c_str(), O_WRONLY);

    while(true)
    {
        if (duty_cycle <= 0)
        {
            // Turn off LED
            write(fd, "0", 1);
            usleep(PWM_PERIOD);
        }
        else if (duty_cycle >= 100)
        {
            // Turn on LED
            write(fd, "1", 1);
            usleep(PWM_PERIOD);
        }
        else
        {   
            std::cout << duty_cycle << std::endl;
            // Turn on LED
            write(fd, "1", 1);
            // Wait for duty cycle
            usleep(100 * duty_cycle);
            // Turn off LED
            write(fd, "0", 1);
            // Wait for remaining duty cycle
            usleep(PWM_PERIOD - 100 * duty_cycle);
        }
    }
    close(fd);
}