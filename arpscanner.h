/*
    Office presence sensor monitoring WiFi devices
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#ifndef ARPSCANNER_H
#define ARPSCANNER_H

#include <iostream>
#include <set>

class ARPScanner
{
public:
    ARPScanner();
    ~ARPScanner();

    bool init(std::string paramaters);

    bool scanLocalNetwork(std::set<std::string>& foundMACAddresses);

    std::string getLastErrorString();

private:
    std::string m_commandString;
    std::string m_lastErrorString;

};

#endif // ARPSCANNER_H
