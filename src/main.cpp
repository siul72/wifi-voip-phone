#include <Arduino.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <TaskScheduler.h>
#include <ESP8266Ping.h>
  
#include "utils.h"

#define  LED_WATCHDOG 2
#define  MAX_DIAL_STRING 64
 

void wifiConnect();
 
void toggleLed();
 

Task wifiConnectTask(1000, TASK_ONCE, &wifiConnect);
Task toggleLedTask(1000, TASK_FOREVER, &toggleLed);
 

WiFiManager wm;
 
char sip_server_str[256];
char sip_port_str[6] = "5060";
char sip_user_name_str[128];
char sip_user_password_str[128];
WiFiManagerParameter sip_server_address_param("server", "sip server", sip_server_str, 256);
WiFiManagerParameter sip_port_param("port", "sip port", sip_port_str, 6);
WiFiManagerParameter sip_user_name_param("auth", "User Name", sip_user_name_str, 128);
WiFiManagerParameter sip_user_password_param("auth", "User Password", sip_user_password_str, 128);

Scheduler runner;
std::vector<String> myQueue;
 
uint16 sip_port;
 
char msg[256];
uint16 signal_was;
 

void testPing(String host){
  if(Ping.ping(host.c_str())) {
    Serial.println("Ping to sip server ok!!");
  } else {
    Serial.println("Ping to sip server nok!!");
  }
}
 
/*****************************************************************
*  Task callback definition
*/
  void toggleLed(){
    digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
    
  }

void connectToMqtt() {
  Serial.println("Connecting to SIP SERVER...");
  digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
  testPing(sip_server_address_param.getValue());

  digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
  //sip.register();
  digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
}


void wifiConnect() {
    digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
    wifiConnectTask.disable();
    runner.deleteTask(wifiConnectTask);
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    //reset settings - wipe credentials for testing
    // wm.resetSettings();
    // set dark theme
    //wm.setClass("invert");
    // add a custom input field
    int customFieldLength = 40;
    new (&sip_server_address_param) WiFiManagerParameter("sip_server_address_id", "Sip Server Address", "10.130.1.100", customFieldLength,"placeholder=\"SIP SERVER address Placeholder\"");
    wm.addParameter(&sip_server_address_param);
    new (&sip_port_param) WiFiManagerParameter("sip_port_id", "SIP Port", "5060", customFieldLength,"placeholder=\"SIP port Placeholder\"");
    wm.addParameter(&sip_port_param);
    new (&sip_user_name_param) WiFiManagerParameter("sip_user_name_id", "Sip User Name", "Manuel das Couves", customFieldLength,"placeholder=\"SIP User Name\"");
    wm.addParameter(&sip_user_name_param);
    new (&sip_user_password_param) WiFiManagerParameter("sip_user_password_id", "Number Digit Port", "123456", customFieldLength,"placeholder=\"Sip User Password\"");
    wm.addParameter(&sip_user_password_param);
    //wm.setSaveParamsCallback(saveParamCallback);
    // auto generated AP name from chipid with password
    bool ret;
    ret = wm.autoConnect();
    if (ret){
      digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
      Serial.println("Wifi Connected");
      
      String sip_server_address = String(sip_server_address_param.getValue());
      //Serial.println("connected1");
      IPAddress ip;
      //ip.fromString(mqtt_address_param.getValue());
      sip_port = atoi(sip_port_param.getValue());
      //dial_timeout =  atoi(dial_timeout_param.getValue());
      //dial_digits = atoi(dial_digits_param.getValue());
      //Serial.println("connected3");
      sprintf(msg, "Try to register to SIP Server address %s:%d", sip_server_address.c_str(), sip_port);
      Serial.println(msg);
      //mqttClient.setServer(ip, mqtt_port);
      //mqttReconnectTask.enable();
    }
 }

 /***********************************************/
    

 void setup() {
       pinMode(LED_WATCHDOG, OUTPUT);
 
       signal_was = LOW;
       
       digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
       Serial.begin(115200);
       
       runner.init();
       Serial.println("Initialized scheduler");
       runner.addTask(wifiConnectTask);
       wifiConnectTask.enable();
       Serial.println("added and enable wifiConnectTask");
       runner.addTask(toggleLedTask);
       Serial.println("Ksoft tone spy started");

  }

void loop() {
      runner.execute();

}
