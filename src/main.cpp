#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>


char *ssid;
char *password;

IPAddress local_IP(192,168,4,1);
IPAddress gateway(192,168,4,254);
IPAddress subnet(255,255,255,0);

ESP8266WebServer    server(80);


void handlePortal();
enum system_state { unknown, booting, wifi_connecting, wifi_ready, wifi_ap_mode};


struct system_configuration {
  enum  system_state state = unknown;
} configuration;

struct settings {
  char ssid[32];
  char password[32];
} user_wifi = {};


unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
unsigned long period = 1000;  //the value is a number of milliseconds
const byte ledPin = D8;    //using the built in LED

void setup() {
  configuration.state = booting;

  Serial.begin(9600);
  Serial.println("Booting");
  sprintf(ssid, "sensor-%08X\n", ESP.getChipId());
  sprintf(password, "123456789");
  
  // start original start
  // WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);
  // while (WiFi.waitForConnectResult() != WL_CONNECTED) {
  //   Serial.println("Connection Failed! Rebooting...");
  //   delay(5000);
  //   ESP.restart();
  // }
  // end original start

  EEPROM.begin(sizeof(struct settings) );
  EEPROM.get( 0, user_wifi );
   
  WiFi.mode(WIFI_STA);
  WiFi.begin(user_wifi.ssid, user_wifi.password);
  
  configuration.state = wifi_connecting;
  byte tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Retry connecting");
    Serial.printf(" ESP8266 Chip id = %08X\n", ESP.getChipId());
    if (tries++ > 15) {
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

      server.on("/", handlePortal);
      server.begin();
      break;
    }
  }

  
  if (WiFi.status() == WL_CONNECTED) {
    configuration.state = wifi_ready;
    Serial.println("wifi_ready set");
  } else {
    Serial.println("wifi is not ready");
  }
  

  pinMode(ledPin, OUTPUT);
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
      Serial.println("started WiFi AP");
    break;

    case wifi_ready:
      ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_FS
        type = "filesystem";
      }

      // NOTE: if updating FS this would be the place to unmount FS using FS.end()
      Serial.println("Start updating " + type);
      });
      ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
      });
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      });
      ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
          Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
          Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
          Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
          Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
          Serial.println("End Failed");
        }
      });
      ArduinoOTA.begin();
      Serial.println("Ready for OTA");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    break;
  
  default:
    break;
  }
  
  
}


void loop() {
  switch (configuration.state)
  {
  case wifi_ready:
    // wifi connected to network. ready
    ArduinoOTA.handle();
    period = 1000; // blink slow
    break;
  case wifi_ap_mode:
    // could not connect, waiting for new configuration.
    server.handleClient();
    period = 200; // blink fast
    break;

  default:
    break;
  }
  
  // Blinking led.

  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  if (currentMillis - startMillis >= period)  //test whether the period has elapsed
  {
    digitalWrite(ledPin, !digitalRead(ledPin));  //if so, change the state of the LED.  Uses a neat trick to change the state
    // Serial.print("IP address: ");
    // Serial.println(WiFi.localIP());
    // Serial.print("AP IP address: ");
    // Serial.println( WiFi.softAPIP());
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