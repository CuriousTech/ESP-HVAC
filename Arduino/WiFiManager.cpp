/**************************************************************
 * WiFiManager is a library for the ESP8266/Arduino platform
 * (https://github.com/esp8266/Arduino) to enable easy
 * configuration and reconfiguration of WiFi credentials and
 * store them in EEPROM.
 * inspired by http://www.esp8266.com/viewtopic.php?f=29&t=2520
 * https://github.com/chriscook8/esp-arduino-apboot
 * Built by AlexT https://github.com/tzapu
 * Licensed under MIT license
 **************************************************************/

// Nextion added for onscreen SSID selection and password entry while server running

#include "WiFiManager.h"
#include "Nextion.h"
#include <TimeLib.h>
#include "eeMem.h"

extern Nextion nex;

WiFiManager::WiFiManager()
{
}

void WiFiManager::autoConnect(char const *apName, const char *pPass) {

  _apName = apName;
  _pPass = pPass;
  //  DEBUG_PRINT("");
  //    DEBUG_PRINT("AutoConnect");

  if ( ee.szSSID[0] ) {
    DEBUG_PRINT("Waiting for Wifi to connect");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ee.szSSID, ee.szSSIDPassword);
    WiFi.setHostname(apName);
    if ( hasConnected() )
    {
      _bCfg = false;
      return;
    }
  }
  //setup AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName);
  DEBUG_PRINT("Started Soft Access Point");
  nex.refreshItem("t0"); // Just to terminate any debug strings in the Nextion
  nex.setPage("SSID");

  DEBUG_PRINT(WiFi.softAPIP());
  DEBUG_PRINT("Don't forget the port #");

  if (!MDNS.begin(apName))
    DEBUG_PRINT("Error setting up MDNS responder!");

  _timeout = true;
  _bCfg = true;
}

boolean WiFiManager::hasConnected(void)
{
  for(int c = 0; c < 50; c++)
  {
    if (WiFi.status() == WL_CONNECTED)
      return true;
    delay(200);
    Serial.print(".");
  }
  DEBUG_PRINT("");
  DEBUG_PRINT("Could not connect to WiFi");
  return false;
}

bool WiFiManager::isCfg(void)
{
  return _bCfg;
}

void WiFiManager::setSSID(int idx){
  WiFi.SSID(idx).toCharArray(ee.szSSID, sizeof(ee.szSSID) );
}

void WiFiManager::setPass(const char *p){
  strncpy(ee.szSSIDPassword, p, sizeof(ee.szSSIDPassword) );
  eemem.update();
  DEBUG_PRINT("Updated EEPROM.  Restaring.");
  autoConnect(_apName, _pPass);
}

void WiFiManager::seconds(void) {
  static int s = 1; // do first list soon

  if(_timeout == false || nex.getPage() != Page_SSID)
    return;
  if(--s)
    return;
  s = 60;
  int n = WiFi.scanNetworks(); // scan for stored SSID each minute
  if(n == 0 )
    return;

  nex.refreshItem("t0"); // Just to terminate any debug strings in the Nextion

  for (int i = 0; i < n; i++)
  {
    if(n < 16)
      nex.btnText(i, WiFi.SSID(i));

    if(WiFi.SSID(i) == ee.szSSID) // found cfg SSID
    {
      nex.setPage("Thermostat"); // set back to normal while restarting
      DEBUG_PRINT("SSID found.  Restarting.");
      autoConnect(_apName, _pPass);
      s = 5; // set to 5 seconds in case it fails again
    }
  }
}

String WiFiManager::page()
{
  String s = HTTP_HEAD;
  s += HTTP_SCRIPT;
  s += HTTP_STYLE;
  s += HTTP_HEAD_END;

  for (int i = 0;  WiFi.SSID(i).length(); ++i)
  {
    DEBUG_PRINT(WiFi.SSID(i));
    DEBUG_PRINT(WiFi.RSSI(i));
    String item = HTTP_ITEM;
    item.replace("{v}", WiFi.SSID(i) );
    s += item;
  }

  String form = HTTP_FORM;
  form.replace("$key", _pPass );
  s += form;
  s += HTTP_END;
  
  _timeout = false;
  return s;
}

String WiFiManager::urldecode(const char *src)
{
    String decoded = "";
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            
            decoded += char(16*a+b);
            src+=3;
        } else if (*src == '+') {
            decoded += ' ';
            *src++;
        } else {
            decoded += *src;
            *src++;
        }
    }
    decoded += '\0';
    
    return decoded;
}
