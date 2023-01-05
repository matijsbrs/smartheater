# Smartheater - ClimatiC
Project for controlling the electric support heaters for our floor heating

## Support for:
Wifi AP mode with dhcp service.
No password for AP to aid with easy setup.
Firmware updates over the air using the ArduinoOTA library

## MQTT support
The device can now be controlled through mqtt

### Configuring
Configuration topic: sensor-00DB056E/cmd 

The message is build as follows:

system=restart
> restart device

settings=read
> read the settings from EEPROM

settings=write
> write settings to EEPROM

settings=default
> load default settings except for network settings and write to EEPROM

settings=show
> return the ip address of the device
> return the post run time of the pump,ac24 and the main cycle interval.
> _topic:_ sensor-00DB056E/settings/ip 192.168.2.89
> _topic:_ sensor-00DB056E/settings/pumpPostRunTime.s 900
> _topic:_ sensor-00DB056E/settings/ac24PostRunTime.s 10
> _topic:_ sensor-00DB056E/settings/period.ms 1000
> _topic:_ sensor-00DB056E/settings/version v1.2.3

floorsetpoint=22.50 
> write 22.5 C as the setpoint temperature for the floor
> max: 25
> min: 0
> default: 22

gcvsetpoint=25.0
> set the Gas CV trigger temperature to 25.0C (when the temperature of 25 or higher is measured the Elements are turned of and the valve is opened and the pump is turned on.
> max: 45
> min: 0
> default: 27

hysteresis=0.5
> the temperature is controlled with 0.5 Degree C as hysteresis
> max: 2.5
> min: 0.1
> default: 0.25

pumpPostRunTime=300
> set the post run time in seconds for the pump to 300 seconds. when the pump is no longer needed. it will turn off afther the postrun time delay has passed. 
> max: 86400
> min: 60
> default: 900

ac24PostRunTime=12
> set the post run time in seconds for the ac24 power supply to 12 seconds. when the pump is no longer needed. it will turn off afther the postrun time delay has passed. 
> max: 86400
> min: 10
> default: 20

period=5000
> Set the main program loop to an interval of 5000 milli seconds.
> max: 10000
> min: 100
> default: 1000

### Output
The device outputs in the influxdb style. 
It can be directly read by telegraf

All output is float notated. 
```
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E Wifi.rssi=-49.000000
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E VloerTemp.c=35.997501
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E AanvoerTemp.c=35.875000
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E VloerSetTemp.c=20.000000
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E AanvoerSetTemp.c=27.000000
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E Hysteresis.c=0.250000
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E Aanvoer.state=1.000000
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E Heater.state=0.000000
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E Pomp.state=1.000000                                                                                       sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E Valve.state=1.000000
sensor-00DB056E/Climate/Floor/state climatic,host=sensor-00DB056E ac24.state=0.000000
```


### Changelog
@050123_V1.2.2
> Update readme file. 
> Added variable validator function
> Added postruntime settings and program interval settings to the mqtt interface.

@050123_V1.2.3
> Update readme file. 
> Management topic changed to sensor-00DB056E/cmd
> system=showsettings changed to settings=show command
> At boot defaults are always first loaded. 
> When connected to a network at boot the settings=show command is exectued once automatically

@050123_V1.2.4
> The settings are pushed to topic: sensor-00DB056E/settings/