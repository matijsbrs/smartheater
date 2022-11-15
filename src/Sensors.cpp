
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Sensors.h"

DeviceAddress T1, T2, T3, T4, T5, T6, T7, T8; // arrays to hold device addresses

// Onewire setup
int oneWireBus = D7;
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

void sensor_setup(int precision)
{
    // Start up the library
    sensors.begin();
    // locate devices on the bus
    Serial.print("Found: ");
    Serial.print(sensors.getDeviceCount(), DEC);
    Serial.println(" Devices.");
    // report parasite power requirements
    Serial.print("Parasite power is: ");
    if (sensors.isParasitePowerMode())
        Serial.println("ON");
    else
        Serial.println("OFF");
    // Search for devices on the bus and assign based on an index.

    if (!sensors.getAddress(T1, 0))
        Serial.println("Not Found Sensor 1");
    if (!sensors.getAddress(T2, 1))
        Serial.println("Not Found Sensor 2");
    if (!sensors.getAddress(T3, 2))
        Serial.println("Not Found Sensor 3");
    if (!sensors.getAddress(T4, 3))
        Serial.println("Not Found Sensor 4");
    if (!sensors.getAddress(T5, 4))
        Serial.println("Not Found Sensor 5");
    if (!sensors.getAddress(T6, 5))
        Serial.println("Not Found Sensor 6");
    if (!sensors.getAddress(T7, 6))
        Serial.println("Not Found Sensor 7");
    if (!sensors.getAddress(T8, 7))
        Serial.println("Not Found Sensor 8");

    // show the addresses we found on the bus
    for (int k = 0; k < sensors.getDeviceCount(); k++)
    {
        Serial.print("Sensor ");
        Serial.print(k + 1);
        Serial.print(" Address: ");
        if (k == 0)
        {
            printAddress(T1);
            Serial.println();
        }
        else if (k == 1)
        {
            printAddress(T2);
            Serial.println();
        }
        else if (k == 2)
        {
            printAddress(T3);
            Serial.println();
        }
        else if (k == 3)
        {
            printAddress(T4);
            Serial.println();
        }
        else if (k == 4)
        {
            printAddress(T5);
            Serial.println();
        }
        else if (k == 5)
        {
            printAddress(T6);
            Serial.println();
        }
        else if (k == 6)
        {
            printAddress(T7);
            Serial.println();
        }
        else if (k == 7)
        {
            printAddress(T8);
            Serial.println();
        }
    }
    // set the resolution to 12 bit per device

    
    sensors.setResolution(T1, precision);
    sensors.setResolution(T2, precision);
    sensors.setResolution(T3, precision);
    sensors.setResolution(T4, precision);
    sensors.setResolution(T5, precision);
    sensors.setResolution(T6, precision);
    sensors.setResolution(T7, precision);
    sensors.setResolution(T8, precision);
    for (int k = 0; k < sensors.getDeviceCount(); k++)
    {
        Serial.print("Sensor ");
        Serial.print(k + 1);
        Serial.print(" Resolution: ");
        if (k == 0)
        {
            Serial.print(sensors.getResolution(T1), DEC);
            Serial.println();
        }
        else if (k == 1)
        {
            Serial.print(sensors.getResolution(T2), DEC);
            Serial.println();
        }
        else if (k == 2)
        {
            Serial.print(sensors.getResolution(T3), DEC);
            Serial.println();
        }
        else if (k == 3)
        {
            Serial.print(sensors.getResolution(T4), DEC);
            Serial.println();
        }
        else if (k == 4)
        {
            Serial.print(sensors.getResolution(T5), DEC);
            Serial.println();
        }
        else if (k == 5)
        {
            Serial.print(sensors.getResolution(T6), DEC);
            Serial.println();
        }
        else if (k == 6)
        {
            Serial.print(sensors.getResolution(T7), DEC);
            Serial.println();
        }
        else if (k == 7)
        {
            Serial.print(sensors.getResolution(T8), DEC);
            Serial.println();
        }
    }
}

void sensor_loop()
{
    // sensors.requestTemperatures();
    // float temperatureC = sensors.getTempCByIndex(0);
    // Serial.print(temperatureC);
    // Serial.println("ºC");

    // call sensors.requestTemperatures() to issue a global temperature request to all devices on the bus
    Serial.print("Reading DATA...");
    sensors.requestTemperatures();
    Serial.println("DONE");
    // print the device information
    for (int k = 0; k < sensors.getDeviceCount(); k++)
    {
        Serial.print("Sensor ");
        Serial.print(k + 1);
        Serial.print(" ");
        if (k == 0)
        {
            printData(T1);
        }
        else if (k == 1)
        {
            printData(T2);
        }
        else if (k == 2)
        {
            printData(T3);
        }
        else if (k == 3)
        {
            printData(T4);
        }
        else if (k == 4)
        {
            printData(T5);
        }
        else if (k == 5)
        {
            printData(T6);
        }
        else if (k == 6)
        {
            printData(T7);
        }
        else if (k == 7)
        {
            printData(T8);
        }
    }
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        // zero pad the address if necessary
        if (deviceAddress[i] < 16)
            Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
    float tempC = sensors.getTempC(deviceAddress);
    Serial.print("Temp : ");
    Serial.print(tempC);
    Serial.print(" Celcius degres ");
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress)
{
}

void printData(DeviceAddress deviceAddress)
{
    Serial.print("Device Address: ");
    printAddress(deviceAddress);
    Serial.print(" ");
    printTemperature(deviceAddress);
    Serial.println();
}