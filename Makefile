CXX = g++
CXXFLAGS = -c -Wall
INCPATH = -I. -Isensor_common -Isensor_common/external/jsoncpp -Isensor_common/external/iniparser
LINK = g++
LIBS = -lbluetooth -lmosquitto -lcurl
TARGET = LANSensor

$(TARGET): iniparser.o main.o arpscanner.o jsoncpp.o datagetter.o lansensor.o mosquittohandler.o dictionary.o
	$(LINK) iniparser.o main.o arpscanner.o jsoncpp.o datagetter.o lansensor.o mosquittohandler.o dictionary.o $(LIBS) -o $(TARGET)

main.o: main.cpp lansensor.h \
		arpscanner.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o main.o main.cpp

arpscanner.o: arpscanner.cpp arpscanner.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o arpscanner.o arpscanner.cpp

lansensor.o: lansensor.cpp lansensor.h \
		arpscanner.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o lansensor.o lansensor.cpp

mosquittohandler.o: sensor_common/mosquittohandler.cpp sensor_common/mosquittohandler.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o mosquittohandler.o sensor_common/mosquittohandler.cpp

datagetter.o: sensor_common/datagetter.cpp sensor_common/datagetter.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o datagetter.o sensor_common/datagetter.cpp

jsoncpp.o: sensor_common/external/jsoncpp/jsoncpp.cpp 
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o jsoncpp.o sensor_common/external/jsoncpp/jsoncpp.cpp

iniparser.o: sensor_common/external/iniparser/iniparser.c sensor_common/external/iniparser/iniparser.h \
		sensor_common/external/iniparser/dictionary.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o iniparser.o sensor_common/external/iniparser/iniparser.c

dictionary.o: sensor_common/external/iniparser/dictionary.c sensor_common/external/iniparser/dictionary.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o dictionary.o sensor_common/external/iniparser/dictionary.c


clean:
	rm -rf *.o $(TARGET)

