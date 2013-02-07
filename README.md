# Office presence LAN sensor
==================================

Requires Linux. Tested with Ubuntu 12.04.

## Dependencies
* [Mosquitto](http://mosquitto.org/)
* [libcurl](http://curl.haxx.se/libcurl/)
* [iniParser](http://ndevilla.free.fr/iniparser/)
* [JsonCpp](http://jsoncpp.sourceforge.net/)

Source codes of **iniParser** and **JsonCpp** are included in subdirectory `sensor_common/external`. The other dependencies can be installed with command

    $ sudo apt-get install libmosquitto0-dev libcurl4-openssl-dev

Program [arp-scan](http://www.nta-monitor.com/tools-resources/security-tools/arp-scan) is also required as it used for the actual MAC scanning. To install it, run command
    
    $ sudo apt-get install arp-scan
    
## Building
Building requires **GNU compiler**. If for some reason it isn't installed already, it can be installed with command

    $ sudo apt-get install build-essential
    
To build the sensor, go to project root directory and run command

    $ make

## Running
MAC scanning requires root priviledges, so the sensor must be started with

    $ sudo ./LANSensor
    
Use **CTRL-C** to quit. Settings can be altered by modifying file `config.ini`.

## License
This software is available under the LGPL license and has been developed by [Nemein](http://nemein.com) as part of the EU-funded [SmarcoS project](http://smarcos-project.eu/).
