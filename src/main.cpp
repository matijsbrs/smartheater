#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <TelnetStream.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>


#include "Credentials.h"
#include "Ota.h"
#include "Controls.h"
#include "Sensors.h"

// function declarations
// void mqtt_callback(char* topic, byte* message, unsigned int length);
void transmit_mqtt(const char * Field, const char * payload);

char ssid[20];
char password[20];

// IPAddress local_IP(192, 168, 4, 1);
// IPAddress gateway(192, 168, 4, 254);
// IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

// MQTT stuff
WiFiClient espClient;
PubSubClient client(espClient);

const char* applicationUUID = "123456789";
const char* default_mqtt_server = "192.168.2.201";
const char* TOPIC = "Climate/Floor";
// MQTT stuff end

#define SENSOR_CV_TEMPERATURE T2
#define SENSOR_VLOER_TEMPERATURE T1

void handlePortal();
enum system_state
{
  unknown,
  booting,
  wifi_reset,
  wifi_connecting,
  wifi_ready,
  wifi_ap_mode
};

struct system_configuration
{
  enum system_state state = unknown;
} configuration;

struct settings
{
  char ssid[32];         // the SSID of the netwerk
  char password[32];     // the WifiPassword
  char energy_topic[32]; // the path to the topic on which the current wattage is found.
  char mqtt_server[32];  // Addres of the MQTT server
  float setPoint;
  float CvOvertemp;
  float hysteresis;
} user_wifi = {};

unsigned long startMillis; // some global variables available anywhere in the program
unsigned long currentMillis;
unsigned long pumpOnMillis; // timestamp when the pump is on.
unsigned long pumpPostRunTime = 900000;  // 900sec / 15min number of milliseconds the pump needs to run after turned off.
unsigned long ac24Millis;
unsigned long ac24PostRunTime = 20000; // keep trafo on for 20sec.
unsigned long period = 1000; // the value is a number of milliseconds

float default_setPoint = 20.0f;
float default_CvOvertemp = 27.0f;
float default_hysteresis = 0.25f;

const byte Led_pin   = D4; // using the built in LED
const byte Pump      = D3;
const byte Heater    = D2; // using the built in LED
const byte ac24      = D6; // using the built in LED
const byte Valve     = D5; // using the built in LED

const byte iotResetPin = D1;


void ApMode()
{
  WiFi.mode(WIFI_AP);
  Serial.print("Setting soft-AP configuration ... ");
  //Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ssid, NULL) ? "Ready" : "Failed!");
  // WiFi.softAP(ssid);
  // WiFi.softAP(ssid, password, channel, hidden, max_connection)

  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());
  
  configuration.state = wifi_ap_mode;
}

void mqtt_callback(char* topic, byte* message, unsigned int length) {
  // Serial.print("Message arrived on topic: ");
  // Serial.print(topic);
  // Serial.print(". Message: ");
  String messageTemp;
  
  // char mytopic[32];
  // snprintf(mytopic, 32, "sensor-%08X/set", ESP.getChipId());

  for (unsigned int i = 0; i < length; i++) {
    // Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  // Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  
    
  if (messageTemp.startsWith("floorsetpoint=")) {
    user_wifi.setPoint = messageTemp.substring(14).toFloat();
    Serial.printf("floor Setpoint: %f\n", user_wifi.setPoint);
  } else if (messageTemp.startsWith("gcvsetpoint=")) {
    user_wifi.CvOvertemp = messageTemp.substring(12).toFloat();
    Serial.printf("gcv Setpoint: %f\n", user_wifi.CvOvertemp);
  } else if (messageTemp.startsWith("hysteresis=")) {
    user_wifi.hysteresis = messageTemp.substring(11).toFloat();
    Serial.printf("hysteresis: %f\n", user_wifi.hysteresis);
  } else if (messageTemp == "settings=read" ) {
    EEPROM.get(0, user_wifi);
  } else if (messageTemp == "settings=write" ) {
       EEPROM.put(0, user_wifi);
    EEPROM.commit();
  } else if (messageTemp == "settings=default" ) {
    user_wifi.setPoint = default_setPoint;
    user_wifi.CvOvertemp = default_CvOvertemp;
    user_wifi.hysteresis = default_hysteresis;
    strcpy(user_wifi.mqtt_server, default_mqtt_server);
    EEPROM.put(0, user_wifi);
    EEPROM.commit();
  } else if (messageTemp == "system=restart" ) {
    ESP.restart();
  } else if (messageTemp == "system=showip" ) {
    char ip[32];
    snprintf(ip, 32, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
    transmit_mqtt("ip",ip);
  }



}

void setup()
{
  pinMode(Heater, OUTPUT);
  pinMode(ac24, OUTPUT);
  pinMode(Valve, OUTPUT);
  pinMode(Pump, OUTPUT);
  pinMode(iotResetPin, INPUT_PULLUP);

  // First enable safe mode
  TurnOff(Heater);
  TurnOn(ac24);
  TurnOn(Valve);
  TurnOn(Pump);

  Serial.begin(9600);
  Serial.println("Booting");

  // start sensors with 12 Bits
  sensor_setup(12);

  configuration.state = booting;
  EEPROM.begin(sizeof(struct settings));

  sprintf(ssid, "sensor-%08X\n", ESP.getChipId());
  sprintf(password, ACCESSPOINT_PASS);

  if (!digitalRead(iotResetPin))
  {
    Serial.println("recover");
    configuration.state = wifi_reset;
    ApMode();
  }
  else
  {
    configuration.state = wifi_connecting;
    // Read network data.
    EEPROM.get(0, user_wifi);

    Serial.printf("Wifi ssid: %s\n", user_wifi.ssid);
    Serial.printf("MQTT host: %s\n", user_wifi.mqtt_server);


    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true); // werkt dit?
    WiFi.begin(user_wifi.ssid, user_wifi.password);
    byte tries = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.println("Retry connecting");
      if (tries++ > 15)
      {
        Serial.println("Failed, start ApMode()");
        ApMode();
        break;
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    configuration.state = wifi_ready;
  }

  startMillis = millis(); // initial start time

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
    Serial.printf("started WiFi AP %s\n", ssid);

    server.on("/", handlePortal);
    server.begin();
    break;

  case wifi_ready:
    setupOTA();
    Serial.println("Ready for OTA");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    // set up mqtt stuff
    client.setServer(user_wifi.mqtt_server, 1883);
    client.setCallback(mqtt_callback);
    char ip[32];
    snprintf(ip, 32, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
    transmit_mqtt("ip",ip);
    break;

  default:
    break;
  }
}



int connect_mqtt() {
  if (client.connected()) {
    return 1;
  }

  Serial.print("Attempting MQTT connection... ");

  // // Create a random client ID
  // String clientId = "ESP8266Client-";
  // sprintf(ssid, "sensor-%08X\n", ESP.getChipId());
  // clientId += String(random(0xffff), HEX);

  // Attempt to connect
  if (client.connect(ssid)) {
    Serial.println("connected");
    char topic[32];
    snprintf(topic, 32, "sensor-%08X/set", ESP.getChipId());
    Serial.println(topic);
    client.subscribe(topic);
    return 1;
  }

  Serial.print("failed, rc=");
  Serial.print(client.state());
  return 0;
}

void transmit_mqtt(const char * Field, const char * payload) {
  if (connect_mqtt()) {
    char topic[75];
    snprintf(topic, 75, "sensor-%08X/%s/%s", ESP.getChipId(), TOPIC,Field);
    // Serial.println(topic);
    client.publish(topic, payload);
  } else {
    Serial.println("MQTT ISSUE!!");
  }
}

void transmit_mqtt_influx(const char * Field, float value) {
  char payload[75];
  snprintf(payload, 75, "climatic,host=sensor-%08X %s=%f",ESP.getChipId(),Field, value);
  transmit_mqtt("state", payload);
}

void transmit_mqtt_float(const char * Field, float value) {
  char payload[20];
  snprintf(payload, 20, "%f",value);
  transmit_mqtt(Field, payload);
}



void ShowClients()
{
  unsigned char number_client;
  struct station_info *stat_info;

  struct ip4_addr *IPaddress;
  IPAddress address;
  int cnt = 1;

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


byte loper = 0x01;

float VloerSetPoint = user_wifi.setPoint;
int ECV_Power = 1;
int GCV_State = 0;
int Pump_Power = 1;
int Valve_State = 1;
int Valve_Previous_state = 0; 
int ac24_Power = 1;

void loop()
{
  switch (configuration.state)
  {
  case wifi_ready:
    // wifi connected to network. ready
    ArduinoOTA.handle();
    break;
  case wifi_ap_mode:
    // could not connect, waiting for new configuration.
    server.handleClient();
    break;

  default:
    break;
  }

 
  client.loop(); // mqtt loop
  // Blinking led.

  currentMillis = millis();                  // get the current "time" (actually the number of milliseconds since the program started)
  if (((currentMillis - startMillis) >= period) && (configuration.state == wifi_ready) )// test whether the period has elapsed
  {
    //sensor_loop(); // show all data from all found sensors. 
    sensors.requestTemperatures();

    float vloerTemp = sensors.getTempC(SENSOR_VLOER_TEMPERATURE ) + 0.31f; // slight offset compensation.
    float cvAanvoerTemp = sensors.getTempC(SENSOR_CV_TEMPERATURE);

    transmit_mqtt_influx("Wifi.rssi",WiFi.RSSI());
    transmit_mqtt_influx("VloerTemp.c",vloerTemp);
    transmit_mqtt_influx("AanvoerTemp.c",cvAanvoerTemp);
    transmit_mqtt_influx("VloerSetTemp.c",user_wifi.setPoint);
    transmit_mqtt_influx("AanvoerSetTemp.c",user_wifi.CvOvertemp);
    transmit_mqtt_influx("Hysteresis.c",user_wifi.hysteresis);


    if ( vloerTemp >= VloerSetPoint ) {
      VloerSetPoint = user_wifi.setPoint - user_wifi.hysteresis;
      ECV_Power = 0; 
      // turn off  
    } else if ( vloerTemp <= VloerSetPoint ) {
      VloerSetPoint = user_wifi.setPoint + user_wifi.hysteresis;
      ECV_Power = 1;
      // turn on
    }
    
    // If water from the Gas CV is warmer that CvOvertemp assume the Room Thermostat has kicked in.
    if ( cvAanvoerTemp > user_wifi.CvOvertemp ) {
      Serial.println("CV heating detected TURN OFF");
      GCV_State = 1;
      transmit_mqtt_influx("Aanvoer.state",1.0f);
      Valve_State = 1;
    } else {
      GCV_State = 0;
      transmit_mqtt_influx("Aanvoer.state",0.0f);
    }

    

    // only when ECV is needed and no Gas CV is active
    if ( (ECV_Power == 1) && (GCV_State == 0) ) {
      TurnOn(Heater);
      transmit_mqtt_influx("Heater.state",1.0f);
      Valve_State = 0;
    } else {
      TurnOff(Heater);
      transmit_mqtt_influx("Heater.state",0.0f);
    }

    // if ECV or GCV is active turn on the pump.
    if ( (ECV_Power == 1 ) || (GCV_State == 1)) {
      Pump_Power = 1;
    } else {
      Pump_Power = 0;
    }



    if ( Pump_Power == 1 ) {
      pumpOnMillis = currentMillis;
      TurnOn(Pump);
      transmit_mqtt_influx("Pomp.state",1.0f);
    } else {
      if ( (currentMillis - pumpOnMillis )  >= pumpPostRunTime ) {
        TurnOff(Pump);
        transmit_mqtt_influx("Pomp.state",0.0f);
      } else {
        transmit_mqtt_influx("Pomp.state",0.5f);
        Serial.println("Pump off delayed.");
      }
    }
    
    if ( Valve_State == 1) {
      TurnOn(Valve);
      transmit_mqtt_influx("Valve.state",1.0f);
    } else if ( Valve_State == 0) {
      TurnOff(Valve);
      transmit_mqtt_influx("Valve.state",0.0f);
    } 

    if ( Valve_State != Valve_Previous_state ) {
      Valve_Previous_state = Valve_State;
      ac24_Power = 1;
    }

    if ( ac24_Power == 1) {
      ac24_Power = 0;
      ac24Millis = currentMillis;
      TurnOn(ac24);
      transmit_mqtt_influx("ac24.state",1.0f);
    } else {
      if ( (currentMillis - ac24Millis )  >= ac24PostRunTime ) {
        TurnOff(ac24);
        transmit_mqtt_influx("ac24.state",0.0f);
      } else {
        TurnOn(ac24);
        transmit_mqtt_influx("ac24.state",1.0f);
      }
    }
    
    
  
    

 
    // digitalWrite(Heater_1, !digitalRead(Heater_1));  //if so, change the state of the LED.  Uses a neat trick to change the state
    if (configuration.state == wifi_ap_mode)
      ShowClients();

    startMillis = currentMillis; // IMPORTANT to save the start time of the current LED state.
  }
}

void handlePortal()
{

  if (server.method() == HTTP_POST)
  {

    strncpy(user_wifi.ssid, server.arg("ssid").c_str(), sizeof(user_wifi.ssid));
    strncpy(user_wifi.password, server.arg("password").c_str(), sizeof(user_wifi.password));
    user_wifi.ssid[server.arg("ssid").length()] = user_wifi.password[server.arg("password").length()] = '\0';

    // load operational defaults
    user_wifi.setPoint = default_setPoint;
    user_wifi.CvOvertemp = default_CvOvertemp;
    user_wifi.hysteresis = default_hysteresis;
    strcpy(user_wifi.mqtt_server, default_mqtt_server);
   
    EEPROM.put(0, user_wifi);
    EEPROM.commit();

    server.send(200, "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device.</p></main></body></html>");
  }
  else
  {

    server.send(200, "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Wifi Setup</title> <style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1 class=''>Wifi Setup</h1><br/><div class='form-floating'><label>SSID</label><input type='text' class='form-control' name='ssid'> </div><div class='form-floating'><br/><label>Password</label><input type='password' class='form-control' name='password'></div><br/><br/><button type='submit'>Save</button><p style='text-align: right'><a href='https://www.mrdiy.ca' style='color: #32C5FF'>mrdiy.ca</a></p></form></main> </body></html>");
  }
}