#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "HTTPUpdateServer.h"
#include <WiFiUdp.h>
#include "NTP.h"
#include "ConfigManager.h"
#include "voipphone.h"
#include "ClickButton.h"

//#define mDNSUpdate(c)  do {} while(0)
using WebServerClass = WebServer;
using HTTPUpdateServerClass = HTTPUpdateServer;

HTTPUpdateServer httpUpdate;
VOIPPhone doorphone;
bool doorphonerunning = false;

ClickButton button1(4, LOW, CLICKBTN_PULLUP);
ClickButton button2(2, LOW, CLICKBTN_PULLUP);
ClickButton button3(15, LOW, CLICKBTN_PULLUP);


//
// Config manager...
//

const char *settingsHTML = (char *)"/settings.html";
const char *stylesCSS = (char *)"/styles.css";
const char *mainJS = (char *)"/main.js";

struct Config {
  char device_name[32];
  char telnr_1[32];
  char telnr_2[32];
  char telnr_3[32];
  char sip_user[32];
  char sip_pass[32];
  char sip_ip[32];
  uint8_t mic_gain;
  uint8_t amp_gain;
  int tz_dst;
  int tz_std;
} config;

struct Metadata {
  int8_t version;
} meta;

ConfigManager configManager;

WiFiUDP wifiUdp;
NTP ntp(wifiUdp);

void APCallback(WebServer *server) {
    server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
        configManager.streamFile(stylesCSS, mimeCSS);
    });

    DebugPrintln(F("AP Mode Enabled. You can call other functions that should run after a mode is enabled ... "));
}


void APICallback(WebServer *server) {
  server->on("/disconnect", HTTPMethod::HTTP_GET, [server](){
    configManager.clearWifiSettings(false);
  });

  server->on("/settings.html", HTTPMethod::HTTP_GET, [server](){
    configManager.streamFile(settingsHTML, mimeHTML);
  });

  // NOTE: css/js can be embedded in a single page HTML
  server->on("/styles.css", HTTPMethod::HTTP_GET, [server](){
    configManager.streamFile(stylesCSS, mimeCSS);
  });

  server->on("/main.js", HTTPMethod::HTTP_GET, [server](){
    configManager.streamFile(mainJS, mimeJS);
  });
  
  server->on("/datetime", HTTPMethod::HTTP_GET, [server](){
    server->send(200, "text/plain", ntp.formattedTime("%d.%m.%Y %X"));
  });
  

  server->on("/dial1", HTTPMethod::HTTP_GET, [server](){
    doorphone.dial(config.telnr_1,config.device_name);
    server->send(200, "text/plain", "ok");
  });

  server->on("/dial2", HTTPMethod::HTTP_GET, [server](){
    doorphone.dial(config.telnr_2,config.device_name);
    server->send(200, "text/plain", "ok");
  });

  server->on("/dial3", HTTPMethod::HTTP_GET, [server](){
    doorphone.dial(config.telnr_3,config.device_name);
    server->send(200, "text/plain", "ok");
  });

  httpUpdate.setup(server);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Startup...");

  pinMode(4,INPUT_PULLUP);
  pinMode(2,INPUT_PULLUP);
  pinMode(15,INPUT_PULLUP);

  strcpy(config.device_name,"\0");
  strcpy(config.telnr_1,"\0");
  strcpy(config.telnr_2,"\0");
  strcpy(config.telnr_3,"\0");
  strcpy(config.sip_ip,"\0");
  strcpy(config.sip_user,"\0");
  strcpy(config.sip_pass,"\0");

  meta.version = 1;
  config.tz_dst = 120;
  config.tz_std = 60;
  // Setup config manager
  configManager.setAPName("voipdoorbell");
  configManager.setAPFilename("/index.html");
  // Settings variables 
  configManager.addParameter("device_name", config.device_name, 32);
  configManager.addParameter("telnr_1", config.telnr_1, 32);
  configManager.addParameter("telnr_2", config.telnr_2, 32);
  configManager.addParameter("telnr_3", config.telnr_3, 32);
  configManager.addParameter("sip_ip", config.sip_ip, 32);
  configManager.addParameter("sip_user", config.sip_user, 32);
  configManager.addParameter("sip_pass", config.sip_pass, 32);
  configManager.addParameter("mic_gain", &config.mic_gain);
  configManager.addParameter("amp_gain", &config.amp_gain);
  configManager.addParameter("tz_dst", &config.tz_dst);
  configManager.addParameter("tz_std", &config.tz_std);
  // Meta Settings
  configManager.addParameter("version", &meta.version, get);
  // Init Callbacks
  configManager.setAPCallback(APCallback);
  configManager.setAPICallback(APICallback);
  configManager.begin(config);

  ntp.ruleDST("CEST", Last, Sun, Mar, 2, config.tz_dst); // last sunday in march 2:00, timezone +120min (+1 GMT + 1h summertime offset)
  ntp.ruleSTD("CET", Last, Sun, Oct, 3, config.tz_std); // last sunday in october 3:00, timezone +60min (+1 GMT)
  ntp.begin();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED ) {
    ntp.update();
    if(strlen(config.sip_ip)<7) {
      //Serial.println("Configuration fault.");
    } else {
      if( !doorphonerunning ) {
        Serial.print("Online. Starting sip modul...");
        if(int result = doorphone.begin(config.sip_ip,config.sip_user,config.sip_pass)==VOIPPHONE_OK) {
          doorphone.setAmpGain(config.amp_gain);
          doorphone.setMicGain(config.mic_gain);
          Serial.println("[OK]");
        } else {
          Serial.print("[ERROR CODE:"+(String)result+"]");
        }
        doorphonerunning = true;
      }
      button1.Update();
      button2.Update();
      button3.Update();
      if(button1.clicks==1) {
        Serial.println("Button1 pressed. Dialing...");
        doorphone.dial(config.telnr_1,config.device_name);
      } else if(button2.clicks==1) {
        Serial.println("Button2 pressed. Dialing...");
        doorphone.dial(config.telnr_2,config.device_name);
      } else if(button3.clicks==1) {
        Serial.println("Button3 pressed. Dialing...");
        doorphone.dial(config.telnr_3,config.device_name);
      }
    }
  }
  
  configManager.loop();
  doorphone.loop();
}