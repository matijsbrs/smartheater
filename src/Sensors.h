#ifndef _SENSORS_H
#define _SENSORS_H

extern DeviceAddress T1, T2, T3, T4, T5, T6, T7, T8; // arrays to hold device addresses
extern int oneWireBus;

void sensor_setup(int precision);
void printData(DeviceAddress deviceAddress);
void printResolution(DeviceAddress deviceAddress);
void printTemperature(DeviceAddress deviceAddress);
void printAddress(DeviceAddress deviceAddress);
void sensor_loop();

#endif