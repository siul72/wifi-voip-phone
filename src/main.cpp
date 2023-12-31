#include <Arduino.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <TaskScheduler.h>
#include <ESP8266Ping.h>
#include <ArduinoSIP.h>
#include <WiFiUdp.h>
  
#include "utils.h"

#define  LED_WATCHDOG 2
#define  MAX_DIAL_STRING 64

 
char acSipIn[2048];
char acSipOut[2048];

Sip SipClient(acSipOut, sizeof(acSipOut));
void wifiConnect();
void toggleLed();
void sipListen();
void sipRegisterClient();

Task wifiConnectTask(1000, TASK_FOREVER, &wifiConnect);
Task sipListenTask(1000, TASK_FOREVER, &sipListen);
Task sipRegisterTask(1000, TASK_FOREVER, &sipRegisterClient);
Task toggleLedTask(1000, TASK_FOREVER, &toggleLed);
 
WiFiManager wifi_manager;
 
 
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

String sip_server_address; 
uint16 sip_server_port;
 
char msg[256];
uint16 signal_was;
uint16 register_count_timeout = 0;
 

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
    
    if (!SipClient.isRegister() && register_count_timeout> 10){
      register_count_timeout = 0;
      sprintf(msg, "Unable to register");
      Serial.println(msg);
      //sipRegisterTask.enable();
      //wm.resetSettings();
      //runner.addTask(wifiConnectTask);
      //wifiConnectTask.enable();
      //toggleLedTask.disable();
      //sipListenTask.disable();
      //sipRegisterTask.disable();

    } else {
      register_count_timeout++;
    }
    

  }
 


void wifiConnect() {
    digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
    wifiConnectTask.disable();
    //runner.deleteTask(wifiConnectTask);
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    //reset settings - wipe credentials for testing
    //wifi_manager.resetSettings();
    // set dark theme
    //wm.setClass("invert");
    // add a custom input field
    int customFieldLength = 40;
    new (&sip_server_address_param) WiFiManagerParameter("sip_server_address_id", "Sip Server Address", "sip.voipbuster.com", customFieldLength,"placeholder=\"SIP SERVER address Placeholder\"");
    wifi_manager.addParameter(&sip_server_address_param);
    new (&sip_port_param) WiFiManagerParameter("sip_port_id", "SIP Port", "5060", customFieldLength,"placeholder=\"SIP port Placeholder\"");
    wifi_manager.addParameter(&sip_port_param);
    new (&sip_user_name_param) WiFiManagerParameter("sip_user_name_id", "Sip User Name", "lutete72", customFieldLength,"placeholder=\"SIP User Name\"");
    wifi_manager.addParameter(&sip_user_name_param);
    new (&sip_user_password_param) WiFiManagerParameter("sip_user_password_id", "Number Digit Port", "LuiCoe@1972", customFieldLength,"placeholder=\"Sip User Password\"");
    wifi_manager.addParameter(&sip_user_password_param);
    //wm.setSaveParamsCallback(saveParamCallback);
    // auto generated AP name from chipid with password
    bool ret;
    ret = wifi_manager.autoConnect();
    if (ret){
      digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
      Serial.println("Wifi Connected");
      
      //String sip_server_address = String(sip_server_address_param.getValue());
      //Serial.println("connected1");
      //IPAddress sip_ip;
      sip_server_address = sip_server_address_param.getValue();
      sip_server_port = atoi(sip_port_param.getValue());
      String sip_user_name =  String(sip_user_name_param.getValue());
      String sip_user_password = String(sip_user_password_param.getValue());
      //Serial.println("connected3");
      
      //mqttClient.setServer(ip, mqtt_port);
      SipClient.Init(sip_server_address.c_str(), sip_server_port, WiFi.localIP().toString().c_str(), sip_server_port, sip_user_name.c_str(), sip_user_password.c_str(), 15);
      toggleLedTask.enable();
      
      Serial.println(msg);
      sipRegisterTask.enable();
    }
 }

 void sipRegisterClient(){
      Serial.println(">>sipRegisterClient");
      sipRegisterTask.disable();
      //runner.deleteTask(sipRegisterTask); 
      //Task sipRegisterTask(1000, TASK_ONCE, &sipRegisterClient);
      //runner.addTask(sip)
      SipClient.Register(); 
      sipListenTask.enable();
      //sipRegisterTask.setInterval(5000);
      //sipRegisterTask.enable();
      Serial.println("<<sipRegisterClient");
 }

 void sipListen(){
  Serial.println(">>sipListen");
  //sipListenTask.disable();
  // SIP processing
  SipClient.Processing(acSipIn, sizeof(acSipIn));
  if(acSipIn[0] != 0) {
     //sipListenTask.disable();
     //acSipIn[0] = 0;
  }
  
  Serial.println("<<sipListen");
 }

 /***********************************************/
    

 void setup() {
       pinMode(LED_WATCHDOG, OUTPUT);
       signal_was = LOW;
       digitalWrite(LED_WATCHDOG, !digitalRead(LED_WATCHDOG));
       Serial.begin(115200);
       runner.init();
       runner.addTask(wifiConnectTask);
       runner.addTask(toggleLedTask);
       runner.addTask(sipListenTask);
       runner.addTask(sipRegisterTask);
       wifiConnectTask.enable();
       Serial.println("Setup ready");
  }

void loop() {
      runner.execute();

}
