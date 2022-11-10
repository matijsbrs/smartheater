#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <TelnetStream.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Credentials.h"
#include "Ota.h"
#include "Controls.h"

char ssid[20];
char password[20];

IPAddress local_IP(192,168,4,1);
IPAddress gateway(192,168,4,254);
IPAddress subnet(255,255,255,0);

ESP8266WebServer    server(80);


void handlePortal();
enum system_state { unknown, booting, wifi_reset, wifi_connecting, wifi_ready, wifi_ap_mode};


struct system_configuration {
  enum  system_state state = unknown;
} configuration;

struct settings {
  char ssid[32];          // the SSID of the netwerk
  char password[32];      // the WifiPassword
  char energy_topic[32];  // the path to the topic on which the current wattage is found.
} user_wifi = {};


unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
unsigned long period = 1000;  //the value is a number of milliseconds

const byte Led_pin = D4; //using the built in LED
const byte Pump = D3;    
const byte Heater_1 = D2;    //using the built in LED
const byte Heater_3 = D6;    //using the built in LED
const byte Heater_2 = D5;    //using the built in LED

const int oneWireBus = D7;  

const byte iotResetPin = D1;

// Onewire setup
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);


void ApMode() {
  WiFi.mode(WIFI_AP);
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ssid,password) ? "Ready" : "Failed!");
  //WiFi.softAP(ssid);
  //WiFi.softAP(ssid, password, channel, hidden, max_connection)
  
  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());
  // WiFi.softAP("Sensor", "admin");
  configuration.state = wifi_ap_mode;
}

void setup() {
  pinMode(Heater_1, OUTPUT);
  pinMode(Heater_2, OUTPUT);
  pinMode(Heater_3, OUTPUT);
  pinMode(Pump, OUTPUT);
  pinMode(iotResetPin, INPUT_PULLUP);

  // First turn every thing off
  TurnOff(Heater_1);
  TurnOff(Heater_2);
  TurnOff(Heater_3);
  TurnOff(Pump);
  
  Serial.begin(9600);
  Serial.println("Booting");

  configuration.state = booting;
  EEPROM.begin(sizeof(struct settings) );
  
  sprintf(ssid, "sensor-%08X\n", ESP.getChipId());
  sprintf(password, ACCESSPOINT_PASS);

  // start the sensor
  sensors.begin();
  

  if (!digitalRead(iotResetPin)) {
    Serial.println("recover");
    configuration.state = wifi_reset;
    ApMode();
  } else {
    configuration.state = wifi_connecting;
    // Read network data.  
    EEPROM.get( 0, user_wifi );
    WiFi.mode(WIFI_STA);
    WiFi.begin(user_wifi.ssid, user_wifi.password);
    byte tries = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Retry connecting");
      if (tries++ > 15) {
        Serial.println("Failed, start ApMode()");
        ApMode();
        break;
      }
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    configuration.state = wifi_ready;
  }
   
  startMillis = millis();  //initial start time
  
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  switch (configuration.state)
  {
    case wifi_connecting:
      Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    break;

    case wifi_ap_mode:
      TurnOn(Led_pin);
      Serial.println("started WiFi AP");
      server.on("/", handlePortal);
      server.begin();
    break;

    case wifi_ready:
      setupOTA();
      Serial.println("Ready for OTA");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    break;
  
  default:
    break;
  }
  
  
}


void ShowClients() 
{
  unsigned char number_client;
  struct station_info *stat_info;
  
  struct ip4_addr *IPaddress;
  IPAddress address;
  int cnt=1;
  
  number_client = wifi_softap_get_station_num();
  stat_info = wifi_softap_get_station_info();
  
  Serial.print("Connected clients: ");
  Serial.println(number_client);

  while (stat_info != NULL)
  {
      IPaddress = &stat_info->ip;
      address = IPaddress->addr;

      Serial.print(cnt);
      Serial.print(": IP: ");
      Serial.print((address));
      Serial.print(" MAC: ");

      uint8_t *p = stat_info->bssid;
      Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", p[0], p[1], p[2], p[3], p[4], p[5]);

      stat_info = STAILQ_NEXT(stat_info, next);
      cnt++;
      Serial.println();
  }
}


void sensor_loop() {
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  Serial.print(temperatureC);
  Serial.println("ºC");
}

byte loper = 0x01;
void loop() {
  switch (configuration.state)
  {
  case wifi_ready:
    // wifi connected to network. ready
    ArduinoOTA.handle();
    period = 150; // blink slow
    break;
  case wifi_ap_mode:
    // could not connect, waiting for new configuration.
    server.handleClient();

    period = 900; // slow blink
    break;

  default:
    break;
  }
  
  // Blinking led.

  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  if (currentMillis - startMillis >= period)  //test whether the period has elapsed
  {
    if ( (loper & 0x02 ) == 0x02) TurnOn(Heater_1); else TurnOff(Heater_1);
    if ( (loper & 0x04 ) == 0x04) TurnOn(Heater_2); else TurnOff(Heater_2);
    if ( (loper & 0x08 ) == 0x08) TurnOn(Heater_3); else TurnOff(Heater_3);
    if ( (loper & 0x01 ) == 0x01) TurnOn(Pump); else TurnOff(Pump);
    
    if ( (loper & 0x04 ) == 0x04) sensor_loop();

    loper++;


    // digitalWrite(Heater_1, !digitalRead(Heater_1));  //if so, change the state of the LED.  Uses a neat trick to change the state
    if (configuration.state == wifi_ap_mode) 
      ShowClients();

    startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }

}


void handlePortal() {

  if (server.method() == HTTP_POST) {

    strncpy(user_wifi.ssid,     server.arg("ssid").c_str(),     sizeof(user_wifi.ssid) );
    strncpy(user_wifi.password, server.arg("password").c_str(), sizeof(user_wifi.password) );
    user_wifi.ssid[server.arg("ssid").length()] = user_wifi.password[server.arg("password").length()] = '\0';
    EEPROM.put(0, user_wifi);
    EEPROM.commit();

    server.send(200,   "text/html",  "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device.</p></main></body></html>" );
  } else {

    server.send(200,   "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title> <style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1 class=''>Wifi Setup</h1><br/><div class='form-floating'><label>SSID</label><input type='text' class='form-control' name='ssid'> </div><div class='form-floating'><br/><label>Password</label><input type='password' class='form-control' name='password'></div><br/><br/><button type='submit'>Save</button><p style='text-align: right'><a href='https://www.mrdiy.ca' style='color: #32C5FF'>mrdiy.ca</a></p></form></main> </body></html>" );
  }
}