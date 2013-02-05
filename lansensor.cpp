/*
    Office presence sensor monitoring LAN devices
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#include "lansensor.h"

#include "json/json.h"
#include "iniparser.h"

#include <iostream>
#include <sstream>
#include <signal.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netdb.h>
#include <stdio.h>

const std::string DEFAULT_ARPSCAN_PARAMETERS = "--localnet";
const std::string DEFAULT_BROKER_ADDRESS = "localhost";
const uint16_t DEFAULT_BROKER_PORT = 1883;
const std::string DEFAULT_DATA_FETCH_URL = "localhost:8181/api/connection";
const int16_t DEFAULT_SCAN_INTERVAL = 10;
const int16_t DEFAULT_CONNECT_ATTEMPT_INTERVAL = 5;

static bool quit = false;

// handler for Ctrl+C, quits program
void siginthandler(int param)
{
    (void)param; //prevent warning

    std::cout << " *** QUITTING... *** " << std::endl;
    quit = true;
}

LANSensor::LANSensor() :
    m_ARPScanner(0),
    m_mosquitto(0),
    m_dataGetter(0),
    m_arpscan_parameters(DEFAULT_ARPSCAN_PARAMETERS),
    m_brokerAddress(DEFAULT_BROKER_ADDRESS),
    m_brokerPort(DEFAULT_BROKER_PORT),
    m_dataFetchUrl(DEFAULT_DATA_FETCH_URL),
    m_scanInterval(DEFAULT_SCAN_INTERVAL),
    m_connectAttemptInterval(DEFAULT_CONNECT_ATTEMPT_INTERVAL),
    m_updateDBNeeded(true)
{
    signal(SIGINT, siginthandler);
}

LANSensor::~LANSensor()
{
    if (m_ARPScanner)
    {
        delete m_ARPScanner;
        m_ARPScanner = 0;
    }
    if (m_dataGetter)
    {
        delete m_dataGetter;
        m_dataGetter = 0;
    }
    if (m_mosquitto)
    {
        delete m_mosquitto;
        m_mosquitto = 0;
    }
}

bool LANSensor::initAll(std::string configFileName)
{
    if (getuid() != 0)
    {
        printError("Root priviledges needed");
        return false;
    }

    bool manualSensorID = false;

    print("Reading config file... ");

    dictionary* ini;
    ini = iniparser_load(configFileName.c_str());
    if (!ini) {
        print("Cannot parse config file. Using default values");
    }
    else
    {
        m_arpscan_parameters = iniparser_getstring(ini, ":arp-scan_parameters",
                                              (char*)DEFAULT_ARPSCAN_PARAMETERS.c_str());
        m_brokerAddress = iniparser_getstring(ini, ":broker_address",
                                              (char*)DEFAULT_BROKER_ADDRESS.c_str());
        m_brokerPort = iniparser_getint(ini, ":broker_port",
                                        DEFAULT_BROKER_PORT);
        m_dataFetchUrl = iniparser_getstring(ini, ":data_fetch_url",
                                              (char*)DEFAULT_DATA_FETCH_URL.c_str());
        m_scanInterval = iniparser_getint(ini, ":scan_interval",
                                        DEFAULT_SCAN_INTERVAL);
        m_connectAttemptInterval = iniparser_getint(ini, ":connect_attempt_interval",
                                        DEFAULT_CONNECT_ATTEMPT_INTERVAL);

        if (iniparser_find_entry(ini, ":sensor_id"))
        {
            m_sensorID = iniparser_getstring(ini, ":sensor_id", 0);
            manualSensorID = true;
        }
    }
    iniparser_freedict(ini);

    if (!manualSensorID)
    {
        // create sensor id from MAC address
        std::string id = getOwnMAC();
        if (!id.size())
        {
            id = "default";
        }
        m_sensorID = "lan-sensor_" + id;
    }

    if (!m_ARPScanner)
    {
        print("Initializing ARP scanner... ");
        m_ARPScanner = new ARPScanner();
        if (!m_ARPScanner->init(m_arpscan_parameters))
        {
            printError(m_ARPScanner->getLastErrorString());
            delete m_ARPScanner;
            m_ARPScanner = 0;
            return false;
        }
    }

    if (!m_dataGetter)
    {
        print("Initializing Curl... ");

        m_dataGetter = new DataGetter();
        if (!m_dataGetter->init())
        {
            printError(m_dataGetter->getLastErrorString());
            return false;
        }
    }

    if (!m_mosquitto)
    {
        print("Initializing Mosquitto... ");

        m_mosquitto = new MosquittoHandler;
        std::string mosqID = m_sensorID;

        if (!m_mosquitto->init(mosqID.c_str()) || !m_mosquitto->connectToBroker(m_brokerAddress.c_str(), m_brokerPort))
        {
            printError(m_mosquitto->getLastErrorString());
            return false;
        }
        m_mosquitto->subscribe("command/fetch_device_database");
        m_mosquitto->subscribe(std::string("command/scan/lan/" + m_sensorID).c_str());
        m_mosquitto->subscribe("command/scan/lan");

        print("Connecting to broker... ");
        if (!m_mosquitto->waitForConnect())
        {
            printError(m_mosquitto->getLastErrorString());
            while (!connectMosquitto(false))
            {
                if (quit) return false;
            }
        }
    }

    m_updateDBNeeded = !updateDeviceData();

    sendHello();

    print("Sensor ID: " + m_sensorID);

    return true;
}

void LANSensor::run()
{
    bool updateDB = false;
    bool scan = false;

    print("Running, CTRL+C to quit...");

    while(!quit)
    {
        // update device db if previous update didn't succeed
        if (m_updateDBNeeded) m_updateDBNeeded = !updateDeviceData();

        checkDevices();

        std::stringstream ss;
        ss << "Waiting scan interval (" << m_scanInterval << " sec)...";
        print(ss.str());

        // wait scan interval, check for incoming messages every second while doing so
        for (int timer = 0; timer < m_scanInterval; timer++)
        {
            sleep(1);

            // check if there are arrived mqtt messages (commands)
            do
            {
                processIncomingMessages(updateDB, scan);
                if (updateDB)
                {
                    m_updateDBNeeded = !updateDeviceData();

                    // exit from wait loop
                    timer = m_scanInterval;
                }
                if (scan)
                {
                    discoverDevices();

                    // exit from wait loop
                    timer = m_scanInterval;
                }
            }

            // check arrived messages again after database update or device discovery,
            // so that a possible request for those is processed before continuing
            while ((updateDB || scan) && !quit);

            // check that Mosquitto is still connected.
            // if Mosquitto disconnects there can be messages from this iteration
            // which aren't sent at all, but missing some outgoing messages
            // shouldn't be too big of a problem.
            if (!m_mosquitto->isConnected())
            {
                printError("Mosquitto disconnected");
                if (connectMosquitto())
                {
                    // update device db and send hello after mosquitto reconnect
                    m_updateDBNeeded = !updateDeviceData();
                    sendHello();
                }
                break;
            }
            if (quit) break;
        }   
    }
}

// checks if devices in the local database exist in the lan.
// sends results using mqtt
bool LANSensor::checkDevices()
{
    print("Checking devices...");
    if (m_devices.size() == 0)
    {
        print("No devices to scan");
        return true;
    }

    std::set<std::string> foundDevices;
    if (!m_ARPScanner->scanLocalNetwork(foundDevices))
    {
        printError(m_ARPScanner->getLastErrorString());
        return false;
    }

    // check if the addresses we are interested in exist in currently found addresses.
    for (unsigned int i = 0; i < m_devices.size(); i++)
    {
        bool available = (foundDevices.find(m_devices.at(i)) != foundDevices.end());

        // send mqtt message based on whether the device is found or not
        std::string availableTopic = "sensor/" + m_sensorID + "/lan/available";
        std::string unavailableTopic = "sensor/" + m_sensorID + "/lan/unavailable";
        std::string topic;

        if (available)
        {
            topic = availableTopic;
            print(m_devices.at(i) + " AVAILABLE");
        }
        else
        {
            topic = unavailableTopic;
            print(m_devices.at(i) + " unavailable");
        }

        m_mosquitto->publish(topic.c_str(), m_devices.at(i).c_str());
        m_mosquitto->loop();
    }
    return true;
}

// gets device info json from server and updates local device database
bool LANSensor::updateDeviceData()
{
    print("Fetching device database...");

    std::string data;
    if (!m_dataGetter->get(m_dataFetchUrl.c_str(), data))
    {
        printError(m_dataGetter->getLastErrorString());
        return false;
    }

    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(data, root);
    if (!parsingSuccessful)
    {
        printError("Failed to parse device data\n" + reader.getFormatedErrorMessages());
        return false;
    }

    m_devices.clear();

    for (unsigned int i = 0; i < root.size(); i++ )
    {
        if (root[i].get("type", "") == "lan")
        {
            std::string newDevice = root[i].get("identifier", "").asString();
            m_devices.push_back(newDevice);
        }
    }

    if (m_devices.size() > 0)
    {
        print("Devices:");
        for (unsigned int i = 0; i < m_devices.size(); i++)
        {
            print(m_devices.at(i));
        }
    }
    else
    {
        print("No devices");
    }

    return true;
}

// checks incoming messages if they contain request for database update or device discovery
void LANSensor::processIncomingMessages(bool& updateDB, bool& scan)
{
    // loop many times to receive multiple messages.
    // can be done smarter with mosquitto 1.0 ->
    for (unsigned int i = 0; i < 10; i++) m_mosquitto->loop();

    std::vector<mqttMessage> messages = m_mosquitto->getArrivedMessages();

    updateDB = false;
    scan = false;

    for (unsigned int i = 0; i < messages.size(); i++)
    {
        if (messages[i].topic == "command/fetch_device_database")
        {
            updateDB = true;
        }
        else if (messages[i].topic.substr(0, 16) == "command/scan/lan")
        {
            scan = true;
        }
    }
}

// scans lan and sends every found device using mqtt
bool LANSensor::discoverDevices()
{
    print("Discovering devices...");

    std::set<std::string> foundDevices;
    if (!m_ARPScanner->scanLocalNetwork(foundDevices))
    {
        printError(m_ARPScanner->getLastErrorString());

        // todo should we inform the server that there is an error
        std::string scanCompleteTopic = "sensor/" + m_sensorID + "/lan/scan_complete";
        m_mosquitto->publish(scanCompleteTopic.c_str(), "");
        m_mosquitto->loop();

        return false;
    }

    if (foundDevices.size() > 0) print("Found: ");

    std::string newDeviceTopic = "sensor/" + m_sensorID + "/lan/new_device";
    for (std::set<std::string>::iterator iter = foundDevices.begin(); iter != foundDevices.end(); ++iter)
    {
        print(*iter);

        Json::Value root;
        root["mac"] = *iter;

        Json::StyledWriter writer;
        std::string JSONstring = writer.write(root);
        m_mosquitto->publish(newDeviceTopic.c_str(), JSONstring.c_str());

        m_mosquitto->loop();
    }

    std::string scanCompleteTopic = "sensor/" + m_sensorID + "/lan/scan_complete";
    m_mosquitto->publish(scanCompleteTopic.c_str(), "");
    m_mosquitto->loop();

    return true;
}

// tries to connect mosquitto when it didn't succeed normally or connection was lost
bool LANSensor::connectMosquitto(bool reconnect)
{
    // when reconnecting the first attempt occures right here at the start before waiting
    if (reconnect)
    {
        print("Reconnecting Mosquitto attempt #1...");
        if (m_mosquitto->reconnect())
        {
            print("Mosquitto reconnected");
            return true;
        }
        if (quit) return false;
    }

    // repeat until connection established or quitted
    int attempts = 1;
    do
    {  
        if (m_connectAttemptInterval > 0)
        {
            std::stringstream ss;
            ss <<  "Waiting connect attempt interval (" << m_connectAttemptInterval << " sec)...";
            print(ss.str());

            // wait for given interval before next attempt, check once a second
            // if user wants to quit
            for (int timer = 0; timer < m_connectAttemptInterval; timer++)
            {
                if (quit) return false;
                sleep(1);
            }
        }

        attempts++;
        std::stringstream ss;
        ss << (reconnect ? "Reconnecting " : "Connecting ");
        ss << "Mosquitto attempt #" << attempts << "...";
        print(ss.str());

        if (m_mosquitto->reconnect()) break;
        if (quit) return false;
    } while (1);

    reconnect ? print("Mosquitto reconnected") : print("Mosquitto connected");
    return true;
}

// sends hello message using mqtt
void LANSensor::sendHello()
{
    std::string helloTopic = "sensor/" + m_sensorID + "/lan/hello";
    m_mosquitto->publish(helloTopic.c_str(), "");
    m_mosquitto->loop();
}

void LANSensor::print(std::string str, bool endl)
{
    std::cout << str;
    endl ? std::cout << std::endl : std::cout << std::flush;
}

void LANSensor::printError(std::string str)
{
    std::cerr << "ERROR: " << str << std::endl;
}

// returns sensor's own MAC address
std::string LANSensor::getOwnMAC()
{
    struct ifreq s;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    std::string mac;

    strcpy(s.ifr_name, "eth0");
    if (ioctl(fd, SIOCGIFHWADDR, &s) == 0)
    {
        std::stringstream ss;
        for (unsigned int i = 0; i < 6; i++)
        {
            // convert MAC to the normal hex form XX:XX:XX:XX:XX:XX
            ss << std::hex << (unsigned int)((unsigned char)s.ifr_addr.sa_data[i]);
            if (i < 5) ss << ":";
        }
        mac = ss.str();
    }
    return mac;
}

