#include <string>
#include "lansensor.h"

int main(int argc, char **argv)
{
    std::string configFileName("config.ini");
    if (argc > 1)
    {
        configFileName = argv[1];
    }
    LANSensor sensor;
    if (!sensor.initAll(configFileName)) return 1;
    sensor.run();

    return 0;
}

