/*
    Office presence sensor monitoring WiFi devices
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#ifndef LANSENSOR_H
#define LANSENSOR_H

#include "mosquittohandler.h"
#include "datagetter.h"

#include "arpscanner.h"

class LANSensor
{
public:
    LANSensor();
    ~LANSensor();

    bool initAll(std::string configFileName);
    void run();

private:

    // gets device info json from server and updates the local device database
    bool updateDeviceData();

    // checks if devices in the local database exist in the lan.
    // sends results using mqtt
    bool checkDevices();

    // checks incoming messages if they contain request for database update or device discovery
    void processIncomingMessages(bool& updateDB, bool& scan);

    // scans lan and sends every found device using mqtt
    bool discoverDevices();

    // tries to connect mosquitto when it didn't succeed normally or connection was lost
    bool connectMosquitto(bool reconnect = true);

    // sends hello message using mqtt
    void sendHello();

    void print(std::string str, bool endl = true);
    void printError(std::string str);

    // returns sensor's own MAC address
    std::string getOwnMAC();

    bool m_running;

    ARPScanner* m_ARPScanner;
    MosquittoHandler* m_mosquitto;
    DataGetter* m_dataGetter;

    std::string m_sensorID;
    std::vector<std::string> m_devices;

    std::string m_arpscan_parameters;
    std::string m_brokerAddress;
    uint16_t m_brokerPort;
    std::string m_dataFetchUrl;
    int16_t m_scanInterval;
    int16_t m_connectAttemptInterval;

    bool m_updateDBNeeded;
};

#endif // LANSENSOR_H
