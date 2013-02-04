/*
    Office presence sensor monitoring WiFi devices
    Copyright (C) 2012-2013 Tuomas Haapala, Nemein <tuomas@nemein.com>
*/

#include "arpscanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <vector>
#include <string>
#include <sstream>

ARPScanner::ARPScanner()
{

}

ARPScanner::~ARPScanner()
{

}

bool ARPScanner::init(std::string paramaters)
{
    FILE *fp;
    int status;

    m_commandString = "arp-scan " + paramaters;

    // use arp-scan with default parameter "-q" and redirect errors to null
    m_commandString += " -q 2> /dev/null";

    fp = popen(m_commandString.c_str(), "r");
    if (fp == NULL)
    {
        m_lastErrorString = "cannot execute arp-scan";
        return false;
    }

    // TODO better checking for status
    status = pclose(fp);
    if (status == -1 || WEXITSTATUS(status) == 127)
    {
        m_lastErrorString = "cannot execute arp-scan";
        return false;
    }

    m_lastErrorString = "";
    return true;
}

bool ARPScanner::scanLocalNetwork(std::set<std::string>& foundMACAddresses)
{
    FILE *fp;
    int status;
    char line[128];

    std::vector<std::string> lines;

    foundMACAddresses.clear();

    // execute arp-scan
    fp = popen(m_commandString.c_str(), "r");
    if (fp == NULL)
    {
        m_lastErrorString = "cannot execute arp-scan";
        return false;
    }

    // parse arp-scan output
    // address information should start on line 3, ignore first 2
    fgets(line, 128, fp);
    fgets(line, 128, fp);

    // go through rest of the output line by line
    while (fgets(line, 128, fp) != NULL)
    {
        // line's second token should be MAC address
        std::istringstream ss(line);
        std::string MACAddress;
        ss >> MACAddress;
        ss >> MACAddress;

        // after address info there comes some other lines, time to quit
        if (MACAddress == "") break;

        foundMACAddresses.insert(MACAddress);
    }

    status = pclose(fp);
    if (status == -1 || WEXITSTATUS(status) != 0) {
        m_lastErrorString = "cannot execute arp-scan";
        return false;
    }

    m_lastErrorString = "";
    return true;
}

std::string ARPScanner::getLastErrorString()
{
    return m_lastErrorString;
}
