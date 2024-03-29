/* This file includes the Co2 sensor based on 44009
   SHT21 Temperature and humidity sensor
   GY49 LUX sensor
   OTA support 
   MQTT support

   Made by Daniel Römer 2019 for homeautomation.
*/

#include <ESP8266WiFi.h>  //For ESP8266
#include <PubSubClient.h> //For MQTT
#include <ESP8266mDNS.h>  //For OTA
#include <WiFiUdp.h>      //For OTA
#include <ArduinoOTA.h>   //For OTA

#include <MAX44009.h>
#include "SparkFun_Si7021_Breakout_Library.h"

#include "MHZ19.h"                                         // include main library
#include <SoftwareSerial.h>   

//MHZ19 CO2 sensor
#define RX_PIN D7                                          // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN D6                                          // Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600                                      // Native to the sensor (do not change)

MHZ19 myMHZ19;                                             // Constructor for MH-Z19 class
SoftwareSerial mySerial(RX_PIN, TX_PIN);                   // Uno example

//WIFI configuration
#define wifi_ssid "Esperyd"
#define wifi_password "Esperyd4"

#define host_name "ESP_Guest1"

//MQTT configuration
#define mqtt_server "192.168.10.100"
#define mqtt_user "mqtt"
#define mqtt_password "mqtt"
String mqtt_client_id="ESP8266-";   //This text is concatenated with ChipId to get unique client_id
//MQTT Topic configuration
String mqtt_base_topic="sensor";
#define humidity_topic "/humidity"
#define temperature_topic "/temperature"
#define lux_topic "/lux"

#define co2_topic "/co2"

//MQTT client
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

//Necesary to make Arduino Software autodetect OTA device
WiFiServer TelnetServer(8266);

MAX44009 light;
Weather sensor;


void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.print(wifi_ssid);
  WiFi.hostname(host_name);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("OK");
  Serial.print("   IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() { 
  Serial.begin(115200);
  Serial.println("\r\nBooting...");
  
  setup_wifi();

  Serial.print("Configuring OTA device...");
  TelnetServer.begin();   //Necesary to make Arduino Software autodetect OTA device  
  ArduinoOTA.setHostname(host_name);
  ArduinoOTA.onStart([]() {Serial.println("OTA starting...");});
  ArduinoOTA.onEnd([]() {Serial.println("OTA update finished!");Serial.println("Rebooting...");});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {Serial.printf("OTA in progress: %u%%\r\n", (progress / (total / 100)));});  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("OK");

  Serial.println("Configuring MQTT server...");
  mqtt_client_id=mqtt_client_id+ESP.getChipId();
  mqtt_base_topic=mqtt_base_topic+"/" + mqtt_client_id;
  mqtt_client.setServer(mqtt_server, 1883);
  Serial.printf("   Server IP: %s\r\n",mqtt_server);  
  Serial.printf("   Username:  %s\r\n",mqtt_user);
  Serial.println("   Cliend Id: "+mqtt_client_id);  
  Serial.println("   MQTT configured!");

  Wire.begin();
  sensor.begin();
  light.begin();

  mySerial.begin(BAUDRATE);                               // Uno example: Begin Stream with MHZ19 baudrate
  myMHZ19.begin(mySerial);                                // *Important, Pass your Stream reference 
  myMHZ19.autoCalibration();                              // Turn auto calibration ON (disable with autoCalibration(false))

  Serial.println("Setup completed! Running app...");
}


void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {    
    if (mqtt_client.connect(mqtt_client_id.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

bool checkBound(float newValue, float prevValue, float maxDiff) {
  return(true);
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}



long now =0; //in ms
long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 1.0;
int min_timeout=6000; //in ms
int CO2 = 0;

float lux = 0;

void loop() {
  
  ArduinoOTA.handle();
  
  if (!mqtt_client.connected()) {
    mqtt_reconnect();
  }
  mqtt_client.loop();

  now = millis();
  if (now - lastMsg > min_timeout) {
    lastMsg = now;
    now = millis();
    //getData();
    float newTemp = sensor.getTemp();
    float newHum = sensor.getRH();
    float lux = light.get_lux();
    CO2 = myMHZ19.getCO2();

    if (checkBound(newTemp, temp, diff)) {
      temp = newTemp;
      Serial.print("Sent ");
      Serial.print(String(temp).c_str());
      Serial.println(" to "+mqtt_base_topic+temperature_topic);
      mqtt_client.publish((mqtt_base_topic+temperature_topic).c_str(), String(temp).c_str(), true);
    }

    if (checkBound(newHum, hum, diff)) {
      hum = newHum;
      Serial.print("Sent ");
      Serial.print(String(hum).c_str());
      Serial.println(" to "+mqtt_base_topic+humidity_topic);
         mqtt_client.publish((mqtt_base_topic+humidity_topic).c_str(), String(hum).c_str(), true);
    }

    mqtt_client.publish((mqtt_base_topic+lux_topic).c_str(), String(lux).c_str(), true);
    if (CO2 > 50 && CO2 < 4900) {
      mqtt_client.publish((mqtt_base_topic+co2_topic).c_str(), String(CO2).c_str(), true);  
    }
    
  }
}
